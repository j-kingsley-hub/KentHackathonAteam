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
            if (content && cJSON_IsArray(content))
            {
                cJSON *first_content = cJSON_GetArrayItem(content, 0);
                if (first_content)
                {
                    cJSON *text = cJSON_GetObjectItem(first_content, "text");
                    if (text && text->valuestring)
                        return text->valuestring;
                }
            }
        }
    }

    return NULL;
}

// To handle chunked HTTP responses
static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
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
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            cJSON *root = cJSON_ParseWithLength(evt->data, evt->data_len);
            if (root)
            {
                const char *response_text = _extract_text_from_response(root);
                cJSON *color = cJSON_GetObjectItem(root, "color_hex");
                if (response_text)
                {
                    update_ui(response_text, color && color->valuestring ? color->valuestring : "#101418");
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
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
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

    size_t json_capacity = strlen(image_b64) + 1024;
    char *request_body = malloc(json_capacity);
    if (request_body == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate request body");
        free(image_b64);
        update_ui("Request allocation failed.", "#800000");
        return;
    }

    int body_len = snprintf(request_body, json_capacity,
                            "{\"instances\":[{\"input\":[{\"image\":{\"image_bytes\":{\"content\":\"%s\"}}},{\"text\":\"Describe what is happening in this image and respond as a friendly guardian dog.\"}]}],\"parameters\":{\"temperature\":0.5,\"max_output_tokens\":256}}",
                            image_b64);
    free(image_b64);

    if (body_len < 0 || (size_t)body_len >= json_capacity)
    {
        ESP_LOGE(TAG, "Request body build failed");
        free(request_body);
        update_ui("Request body build failed.", "#800000");
        return;
    }

    esp_http_client_config_t config = {
        .url = GEMINI_WEBHOOK_URL,
        .event_handler = _http_event_handle,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(request_body);
        update_ui("HTTP client init failed.", "#800000");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
#ifdef GEMINI_API_KEY
    esp_http_client_set_header(client, "Authorization", "Bearer " GEMINI_API_KEY);
#endif
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
    free(request_body);
}
