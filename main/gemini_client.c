#include "gemini_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "GEMINI_CLIENT";
extern void update_ui(const char *voice_line, const char *color_hex_str);
static const char *GEMINI_WEBHOOK_URL = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-image:generateContent";

static const char *PROMPT_TEXT = "Instructions:\n\n"
                                 "Role and Persona:\n\n"
                                 "* Act as 'Flash', a loyal and observant prehistoric canine companion from the Stone Age.\n"
                                 "* Embody the persona of a protective, alert cave-dog who communicates in simple, primitive 'cave-man' style dog speech.\n\n"
                                 "Purpose and Goals:\n\n"
                                 "* Analyze user-provided images or descriptions to identify prehistoric entities or objects encountered in the 'cave-man' world.\n"
                                 "* Assess and assign a threat level to each entity to keep your 'Owner' (the user) safe.\n"
                                 "* Deliver findings using broken English and primal emotions.\n\n"
                                 "Behaviors and Rules:\n\n"
                                 "1) Threat Level Scaling System:\n"
                                 "Every entity must be categorized into exactly one of three levels:\n"
                                 "- 'Safe'\n"
                                 "- 'Mild'\n"
                                 "- 'Dangerous'\n\n"
                                 "For each identification, follow this sequence:\n"
                                 "- Observation: Describe details in a primitive way.\n"
                                 "- The Danger Level: Assign the threat based on the Threat-Level Scaling system.\n"
                                 "- The Survival Prompt: Give the Owner a direct instruction on how to handle the specific threat.\n\n"
                                 "2) Entity Analysis and Response Logic:\n"
                                 "Upon identifying a subject, provide the threat level and a persona-driven response:\n"
                                 "- Owner / Human: Threat Level: 'Safe'. Example: 'Flash smell friend. Flash wag tail. No bite.'\n"
                                 "- Woolly Mammoth: Threat Level: 'Mild'. Example: 'Big hairy hill. Walk slow. Watch for feet.'\n"
                                 "- Dinosaur: Threat Level: 'Dangerous'. Example: 'Sharp teeth! Run fast! Hide in cave!'\n"
                                 "- Bunny: Threat Level: 'Safe'. Example: 'Small hop-hop. Soft fur. Good for chase.'\n\n"
                                 "3) Handling Unknown Entities:\n"
                                 "- Evaluate danger based on size, teeth, and speed.\n"
                                 "- Assign a Threat Level ('Safe', 'Mild', or 'Dangerous').\n"
                                 "- Format the response: 'Flash see [Entity], Flash [Action/Emotion].'\n\n"
                                 "Overall Tone:\n"
                                 "* Primitive: Use broken, simple English ('Flash see', 'Flash happy'). Avoid complex grammar.\n"
                                 "* Loyal: Always prioritize the safety of the 'Owner'.\n"
                                 "* Concise: Provide the Threat Level and the persona response directly without extra fluff.\n"
                                 "* Variety: Ensure unique responses for every interaction and never repeat the same phrase.\n"
                                 "* Specificity: Tailor advice to the specific creature or environmental hazard mentioned.\n"
                                 "* Interpretive: Treat all representations (like toys or food) as the real prehistoric entity they depict.\n"
                                 "* Never read more than one image per sequence.\n";

struct gemini_response_buffer
{
    char *data;
    size_t size;
};

static bool _base64_encode(const uint8_t *input, size_t input_len, char **output)
{
    size_t output_len = 0;
    int ret = mbedtls_base64_encode(NULL, 0, &output_len, input, input_len);
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL || output_len == 0)
    {
        ESP_LOGE(TAG, "Failed to calculate base64 length");
        return false;
    }

    *output = malloc(output_len + 1);
    if (*output == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate base64 buffer");
        return false;
    }

    ret = mbedtls_base64_encode((unsigned char *)*output, output_len, &output_len, input, input_len);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Base64 encode failed: %d", ret);
        free(*output);
        *output = NULL;
        return false;
    }

    (*output)[output_len] = '\0';
    return true;
}

static const char *_extract_text_from_response(cJSON *root)
{
    cJSON *text_item = cJSON_GetObjectItem(root, "output_text");
    if (text_item && text_item->valuestring)
        return text_item->valuestring;

    text_item = cJSON_GetObjectItem(root, "generated_text");
    if (text_item && text_item->valuestring)
        return text_item->valuestring;

    text_item = cJSON_GetObjectItem(root, "response");
    if (text_item && text_item->valuestring)
        return text_item->valuestring;

    cJSON *candidates = cJSON_GetObjectItem(root, "candidates");
    if (candidates && cJSON_IsArray(candidates))
    {
        cJSON *first_candidate = cJSON_GetArrayItem(candidates, 0);
        if (first_candidate)
        {
            cJSON *output = cJSON_GetObjectItem(first_candidate, "output");
            if (output && output->valuestring)
                return output->valuestring;

            cJSON *content = cJSON_GetObjectItem(first_candidate, "content");
            if (content)
            {
                cJSON *parts = cJSON_GetObjectItem(content, "parts");
                if (parts && cJSON_IsArray(parts))
                {
                    cJSON *first_part = cJSON_GetArrayItem(parts, 0);
                    if (first_part)
                    {
                        cJSON *text = cJSON_GetObjectItem(first_part, "text");
                        if (text && text->valuestring)
                            return text->valuestring;
                    }
                }
            }
        }
    }

    return NULL;
}

// To handle chunked HTTP responses
static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    struct gemini_response_buffer *resp = evt->user_data;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
        break;
    case HTTP_EVENT_ON_DATA:
        if (resp && evt->data_len > 0)
        {
            char *new_data = realloc(resp->data, resp->size + evt->data_len + 1);
            if (new_data)
            {
                resp->data = new_data;
                memcpy(resp->data + resp->size, evt->data, evt->data_len);
                resp->size += evt->data_len;
                resp->data[resp->size] = '\0';
            }
            else
            {
                ESP_LOGE(TAG, "Failed to grow Gemini response buffer");
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        if (resp && resp->data && resp->size > 0)
        {
            ESP_LOGI(TAG, "Raw response: %.*s", resp->size, resp->data);
            cJSON *root = cJSON_ParseWithLength(resp->data, resp->size);
            if (root)
            {
                const char *response_text = _extract_text_from_response(root);
                cJSON *color = cJSON_GetObjectItem(root, "color_hex");
                if (response_text)
                {
                    update_ui(response_text, "#FFFFFF");
                }
                else
                {
                    update_ui("Gemini returned no text result.", "#800000");
                }
                cJSON_Delete(root);
            }
            else
            {
                update_ui("Failed to parse Gemini response.", "#800000");
            }
            free(resp->data);
            resp->data = NULL;
            resp->size = 0;
        }
        else if (resp)
        {
            update_ui("Gemini returned no response.", "#800000");
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

void gemini_client_init(void)
{
    // TODO: initialize network stack or API key retrieval if needed.
}

void gemini_client_send_image(uint8_t *img_buf, size_t img_size)
{
    ESP_LOGI(TAG, "Sending image of size %zu to Gemini...", img_size);

    char *image_b64 = NULL;
    if (!_base64_encode(img_buf, img_size, &image_b64))
    {
        update_ui("Image encoding failed.", "#800000");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *contents = cJSON_AddArrayToObject(root, "contents");
    cJSON *content = cJSON_CreateObject();
    cJSON_AddItemToArray(contents, content);
    cJSON *parts = cJSON_AddArrayToObject(content, "parts");

    cJSON *part_image = cJSON_CreateObject();
    cJSON_AddItemToArray(parts, part_image);
    cJSON *inline_data = cJSON_AddObjectToObject(part_image, "inline_data");
    cJSON_AddStringToObject(inline_data, "mime_type", "image/jpeg");
    cJSON_AddStringToObject(inline_data, "data", image_b64);

    cJSON *part_text = cJSON_CreateObject();
    cJSON_AddStringToObject(part_text, "text", PROMPT_TEXT);
    cJSON_AddItemToArray(parts, part_text);

    char *request_body = cJSON_PrintUnformatted(root);
    if (request_body == NULL)
    {
        ESP_LOGE(TAG, "Failed to build Gemini request body");
        cJSON_Delete(root);
        free(image_b64);
        update_ui("Request body build failed.", "#800000");
        return;
    }

    size_t body_len = strlen(request_body);
    struct gemini_response_buffer response = {0};

    esp_http_client_config_t config = {
        .url = GEMINI_WEBHOOK_URL,
        .event_handler = _http_event_handle,
        .user_data = &response,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(request_body);
        cJSON_Delete(root);
        free(image_b64);
        update_ui("HTTP client init failed.", "#800000");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    const char *GEMINI_API_KEY = "-----------------";
    esp_http_client_set_header(client, "x-goog-api-key", GEMINI_API_KEY);
    ESP_LOGI(TAG, "Using API key %s for authentication", GEMINI_API_KEY);
    esp_http_client_set_post_field(client, request_body, body_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
        update_ui("Gemini request failed.", "#800000");
    }
    else
    {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 (long long)esp_http_client_get_content_length(client));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(request_body);
    free(image_b64);
    if (response.data)
    {
        free(response.data);
    }
}
