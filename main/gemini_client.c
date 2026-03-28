#include "gemini_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "GEMINI_CLIENT";
extern void update_ui(const char *voice_line, const char *color_hex_str);
static const char *GEMINI_WEBHOOK_URL = "http://your-server-or-proxy.com/gemini-gem-webhook";

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
            // Here we would parse the JSON response from the Gem.
            // Assuming it returns: {"threat": "...", "color_hex": "...", "voice_line": "..."}
            cJSON *root = cJSON_Parse(evt->data);
            if (root)
            {
                cJSON *voice_line = cJSON_GetObjectItem(root, "voice_line");
                cJSON *color = cJSON_GetObjectItem(root, "color_hex");
                if (voice_line && voice_line->valuestring)
                {
                    ESP_LOGI(TAG, "DOG SAYS: %s", voice_line->valuestring);
                    update_ui(voice_line->valuestring, color ? color->valuestring : NULL);
                }
                cJSON_Delete(root);
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
    // Initialize Wi-Fi connection (usually handled by a separate wifi manager component in real app)
}

void gemini_client_send_image(uint8_t *img_buf, size_t img_size)
{
    ESP_LOGI(TAG, "Sending image of size %zu to Gemini...", img_size);

    esp_http_client_config_t config = {
        .url = GEMINI_WEBHOOK_URL,
        .event_handler = _http_event_handle,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return;
    }

    // Set headers for multipart form data or base64 json depending on your webhook/API implementation
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char *)img_buf, img_size);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 (long long)esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}
