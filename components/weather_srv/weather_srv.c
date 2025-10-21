#include <stdio.h>
#include "esp_http_client.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <sys/param.h>
#include <string.h>


#include "weather_srv.h"
#include "event.h"
#include "wifi_connect.h"

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static const char *TAG = "weather_srv";

// http response data max length
#define HTTP_RES_MAX_LEN 2048

#define MAX_HTTP_OUTPUT_BUFFER 2048

// Today's weather API, location Wuhan
#define TODAY_WEATHER_URL_PREFIX    "https://api.seniverse.com/v3/weather/now.json?key=SFj3LEY7zNwXAZt1V&location="
#define TODAY_WEATHER_URL_SUFFIX    "&language=en&unit=c"

// Weather API for the next three days, location: Wuhan
#define _3_TODAY_WEATHER_URL_PREFIX    "https://api.seniverse.com/v3/weather/daily.json?key=SFj3LEY7zNwXAZt1V&location="
#define _3_TODAY_WEATHER_URL_SUFFIX    "&language=en&unit=c&start=0&days=5"

weather_data_t today_weather;


SemaphoreHandle_t weather_request_mutex = NULL;          // Mutex protects the state and WiFi operations to ensure reliable changes to current_wifi_state.

static char final_today_weather_url[256];

static char final_3_today_weather_url[256];

// Default query: Wuhan weather
char weather_position[32]="wuhan";


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

static void user_weather_srv_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void user_weather_pos_change_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);


esp_err_t _http_event_handler(esp_http_client_event_t *evt);

void parse_3day_weather_json(const char *json_str);

void parse_today_weather_json(const char *json_str);

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


/**
 * @brief Register weather-related event handlers with the UI event loop.
 *
 * Registers handlers for weather request and weather position change events
 * so the application can trigger weather queries and update the configured
 * location.
 */
void weather_register_weathe_service_handler(void)
{
    // Register SNTP request handler
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WEATHER_REQUEST, user_weather_srv_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WEATHER_POSITION_CHANGE, user_weather_pos_change_handler, NULL)); 


}

/**
 * @brief Handle APP_WEATHER_REQUEST events to fetch current and multi-day weather.
 *
 * This handler validates WiFi connectivity, rate-limits requests, constructs
 * the HTTP URLs for current and multi-day forecasts, performs HTTPS GET
 * requests using the esp_http_client, parses the JSON responses, and posts
 * an APP_WEATHER_RESPONSE event with parsed weather data.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base (APP_EVENT).
 * @param event_id Event identifier (APP_WEATHER_REQUEST).
 * @param event_data Pointer to any event-specific data (unused).
 */
static void user_weather_srv_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (current_wifi_state != WIFI_STATE_GETIP)
    {
        ESP_LOGE(TAG, "please connect wifi first ");
        return ;
    }
    
    // The tick value when the weather request was last sent
    static TickType_t last_tick=0;

    // Current tick value
    TickType_t now_tick=xTaskGetTickCount();

    // If two weather requests are sent within less than 6000*10 ms = 60 seconds, ignore the second request directly.
    if(now_tick -last_tick<1000)
    {
        ESP_LOGE(TAG,"request weather too frequency ");
        return ;
    }

    // Change the previous tick value before successfully sending the weather request.
    last_tick= now_tick;

    //set http client access url 
    snprintf(final_today_weather_url, sizeof(final_today_weather_url), "%s%s%s", TODAY_WEATHER_URL_PREFIX, weather_position, TODAY_WEATHER_URL_SUFFIX);
    snprintf(final_3_today_weather_url, sizeof(final_3_today_weather_url), "%s%s%s", _3_TODAY_WEATHER_URL_PREFIX, weather_position, _3_TODAY_WEATHER_URL_SUFFIX);


    char *weather_response_buffer = malloc(HTTP_RES_MAX_LEN + 1);

    esp_http_client_config_t config =
    {
            .url = final_today_weather_url,
            .event_handler = _http_event_handler,
            .user_data = weather_response_buffer, // Pass address of local buffer to get response,store response data

            .keep_alive_enable = false, // configure  tcp connection keep alive attribute (note: not configure http keep alive)

            .transport_type = HTTP_TRANSPORT_OVER_SSL, // https

            .crt_bundle_attach = esp_crt_bundle_attach, // Use the official built-in set of trusted root certificates (CAs) to verify the legitimacy of the certificates provided by HTTPS websites.
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET method ,default blocking execute
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %" PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    // ESP_LOGI("mytest", "receive response %s", weather_response_buffer);

    // Store the important information about today's weather in the global variable today_weather.
    parse_today_weather_json(weather_response_buffer);


    // To access the weather for the past three days, simply change the URL you are accessing.
    esp_http_client_set_url(client,final_3_today_weather_url);

    // This function defaults to blocking execution until completion before returning.
    err = esp_http_client_perform(client);
    if (err == ESP_OK) 
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else 
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // ESP_LOGI("mytest", "receive response %s",weather_response_buffer);

    parse_3day_weather_json(weather_response_buffer);

    ESP_LOGW(TAG,"close http connec");
    
    // Close the HTTP connection
    esp_http_client_cleanup(client);

    //send weather response date
    esp_event_post_to(ui_event_loop_handle,APP_EVENT,APP_WEATHER_RESPONSE, &today_weather,sizeof(today_weather),portMAX_DELAY);

    free(weather_response_buffer);



}
/**
 * @brief Handle weather position change events.
 *
 * Updates the internally constructed weather query URLs based on the global
 * `weather_position` string and logs the new position. This function is
 * invoked when APP_WEATHER_POSITION_CHANGE is posted.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base (APP_EVENT).
 * @param event_id Event identifier (APP_WEATHER_POSITION_CHANGE).
 * @param event_data Event-specific data (unused).
 */
static void user_weather_pos_change_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    snprintf(final_today_weather_url, sizeof(final_today_weather_url), "%s%s%s", TODAY_WEATHER_URL_PREFIX, weather_position, TODAY_WEATHER_URL_SUFFIX);
    snprintf(final_3_today_weather_url, sizeof(final_3_today_weather_url), "%s%s%s", _3_TODAY_WEATHER_URL_PREFIX, weather_position, _3_TODAY_WEATHER_URL_SUFFIX);


    ESP_LOGI(TAG,"new weather position is %s ",weather_position);


}



/**
 * @brief HTTP event handler used by esp_http_client for weather requests.
 *
 * Handles HTTP lifecycle events (connect, headers, data chunks, finish,
 * disconnect and redirect) and accumulates response data into either the
 * user-provided buffer or an internally allocated buffer. This handler
 * supports non-chunked responses and frees internal buffers on finish.
 *
 * @param evt Pointer to the esp_http_client_event_t describing the event.
 * @return ESP_OK on success, or ESP_FAIL on allocation/read errors.
 */
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;

        // The client sends a completion request header field to the server.
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "client finish send headers");
        break;

        // Receiving response header fields from the server.
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;

        // Occurred when receiving data from the server
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        
        // Clean the buffer in case of a new request
        if (output_len == 0 && evt->user_data)
        {
            // we are just starting to copy the output data into the use
            memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
        }
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            if (evt->user_data)
            {
                // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len)
                {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            }
            else
            {
                int content_len = esp_http_client_get_content_length(evt->client);
                if (output_buffer == NULL)
                {
                    // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                    output_buffer = (char *)calloc(content_len + 1, sizeof(char));
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                copy_len = MIN(evt->data_len, (content_len - output_len));
                if (copy_len)
                {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                }
            }
            output_len += copy_len;
        }

        break;

        // Complete the HTTP session
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL)
        {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
        // Client-server connection disconnected
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;

        // Handle redirect events
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}




/**
 * @brief Parse JSON response for current weather and populate global data.
 *
 * Parses the JSON string returned by the weather API for current conditions,
 * extracts the location name, textual weather description, temperature and
 * weather code, and stores them into the global `today_weather` structure.
 *
 * @param json_str Null-terminated JSON string containing the API response.
 */
void parse_today_weather_json(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
    {
        printf("Parse error: %s\n", cJSON_GetErrorPtr());
        return;
    }

    // results is an array
    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (cJSON_IsArray(results))
    {
        cJSON *first_result = cJSON_GetArrayItem(results, 0); // Get the first element

        // location object
        cJSON *location = cJSON_GetObjectItem(first_result, "location");
        if (cJSON_IsObject(location))
        {
            cJSON *name = cJSON_GetObjectItem(location, "name");

            // Position
            strcpy(today_weather.location, name->valuestring);
        }

        // now object
        cJSON *now = cJSON_GetObjectItem(first_result, "now");
        if (cJSON_IsObject(now))
        {
            cJSON *text = cJSON_GetObjectItem(now, "text");
            cJSON *temperature = cJSON_GetObjectItem(now, "temperature");
            cJSON *code=cJSON_GetObjectItem(now,"code");

            // Weather string of the day
            strcpy(today_weather.today_weather, text->valuestring);

            // Temperature of the day
            strcpy(today_weather.today_tem, temperature->valuestring);

            char* endptr;
            today_weather.today_weather_code= strtol(code->valuestring, &endptr, 10); // Decimal system
                                
            if (*endptr != '\0') 
            {
                printf("leaved unconverted char: %s\n", endptr);
            }
        }
    }
    cJSON_Delete(root);
}

/**
 * @brief Parse JSON response for recent 3 day weather data
 *
 * Parses the JSON string returned by the weather API for current conditions,
 * extracts the location name, textual weather description, temperature and
 * weather code.
 *
 * @param json_str Null-terminated JSON string containing the API response.
 */
void parse_3day_weather_json(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
    {
        printf("Parse error: %s\n", cJSON_GetErrorPtr());
        return;
    }

    // results array
    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (cJSON_IsArray(results))
    {
        cJSON *first_result = cJSON_GetArrayItem(results, 0);

        // 1. location
        cJSON *location = cJSON_GetObjectItem(first_result, "location");
        if (cJSON_IsObject(location))
        {
            cJSON *name = cJSON_GetObjectItem(location, "name");

            // Compare the current obtained position with the previously obtained position to check if they are the same.
            if( strcmp(today_weather.location,name->valuestring ) !=0)
            {
                strcpy(today_weather.location, name->valuestring);
                
            }
        }

        // 2. Daily Forecast
        cJSON *daily = cJSON_GetObjectItem(first_result, "daily");
        if (cJSON_IsArray(daily))
        {
            int size = cJSON_GetArraySize(daily);
            char* endptr;
            for (int i = 0; i < size; i++)
            {
                cJSON *day = cJSON_GetArrayItem(daily, i);

                cJSON *text_day = cJSON_GetObjectItem(day, "code_day");
                cJSON *high = cJSON_GetObjectItem(day, "high");
                cJSON *low = cJSON_GetObjectItem(day, "low");

                snprintf(today_weather._3day_tem_range[i], sizeof(today_weather._3day_tem_range[i]), "%s-%s", low->valuestring, high->valuestring);

                
                today_weather._3day_weather[i]= strtol(text_day->valuestring, &endptr, 10); // Decimal system
                                
                if (*endptr != '\0') 
                {
                    printf("leaved unconverted char : %s\n", endptr);
                }


            }
        }

    }

    cJSON_Delete(root);
}
