#include <stdio.h>
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_err.h"
#include "nvs_flash.h"


#include "wifi_connect.h"
#include "event.h"



//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------
static const char *TAG = "WIFI_TASK";



// Global variable
wifi_state_t current_wifi_state = WIFI_STATE_CLOSED;
SemaphoreHandle_t wifi_mutex = NULL;          // Mutex protects the state and WiFi operations to ensure reliable changes to current_wifi_state.

static esp_event_handler_instance_t wifi_event_instance;    // WiFi System Event Example
static esp_event_handler_instance_t ip_event_instance;      // IP Event Example


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static void wifi_system_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);


static void user_wifi_close_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// User requests scan
static void user_wifi_start_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// User requests connection
static void user_wifi_connect_request_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


/**
 * @brief WiFi management task entry point.
 *
 * Creates the WiFi mutex, initializes the default event loop and network
 * interface, registers system and IP event handlers, and registers UI event
 * handlers for scan, connect and close requests. This task sets up the
 * asynchronous event-driven WiFi control used by the application.
 *
 * @param Par Unused task parameter.
 */
void wifi_task(void *Par) 
{
    // Initialize mutex
    wifi_mutex = xSemaphoreCreateMutex();
    if (wifi_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi mutex");
        vTaskDelete(NULL);
        return;
    }

    // Create the default event loop (WiFi system events)
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Register WiFi system event handler (handles asynchronous events such as START, STOP, etc.)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_system_event_handler, NULL, &wifi_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_system_event_handler, NULL, &ip_event_instance));

    // APP_WIFI_SCAN_START
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_SCAN_START, user_wifi_start_handler, NULL));
    
    // APP_WIFI_CONNECT_REQUEST
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_CONNECT_REQUEST, user_wifi_connect_request_handler, NULL));

    // APP_WIFI_CLOSE
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_CLOSE, user_wifi_close_handler, NULL));





    // Cleanup (theoretically should never be reached)
    vTaskDelete(NULL);
}


/**
 * @brief System event handler for WiFi and IP events.
 *
 * Handles asynchronous WiFi and IP events dispatched by the ESP event loop
 * (e.g. STA start/stop, scan completion, connection, disconnection and
 * IP acquisition). This function updates the global @c current_wifi_state,
 * posts UI events for scan results or disconnection notifications, and
 * performs cleanup when WiFi is stopped as part of deinitialization.
 *
 * @param arg Handler argument (unused).
 * @param event_base Event base (WIFI_EVENT or IP_EVENT).
 * @param event_id Event identifier describing the specific event.
 * @param event_data Event-specific data pointer (type varies by event).
 */
static void wifi_system_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) 
{
    if (xSemaphoreTake(wifi_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Mutex take timeout in START handler");
        return ;

    }

    // Handle system WiFi-related events
    if (event_base == WIFI_EVENT ) 
    {
       switch (event_id) 
       {
        //esp_wifi_start() successfully call
        case WIFI_EVENT_STA_START:

            if(current_wifi_state == WIFI_STATE_INITING) 
            {
                current_wifi_state = WIFI_STATE_IDLE;
                ESP_LOGI(TAG, "WiFi started successfully, state -> IDLE");
                
                //automatic scan nearby wifi
                ESP_LOGI(TAG,"wifi scan begin,idle -> busy");
                current_wifi_state = WIFI_STATE_BUSY;
                esp_wifi_scan_start(NULL, false);  // Non-blocking

                
            }
            else
            {
                ESP_LOGE(TAG,"illegal state =%d",current_wifi_state);

            }

            break;
        
        //sta stop event
        case WIFI_EVENT_STA_STOP:
            
            // esp_wifi_stop() succeeded, allowing deinit.
            if (current_wifi_state == WIFI_STATE_CLOSING) 
            {
                ESP_LOGI(TAG, "WiFi stopped, proceeding to deinit");
                esp_wifi_restore();  // Reset configuration
                esp_wifi_deinit();
                esp_netif_deinit();
                current_wifi_state = WIFI_STATE_CLOSED;
                ESP_LOGI(TAG, "WiFi deinit complete, state -> CLOSED");

            }
            break;    
        
        //sta scan done event 
        case WIFI_EVENT_SCAN_DONE:

            if (current_wifi_state == WIFI_STATE_BUSY) 
            {
                current_wifi_state = WIFI_STATE_IDLE;
                ESP_LOGI(TAG, "Scan done, state -> IDLE");
                
                // Get the total number of scanned APs
                uint16_t ap_total_count;
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_total_count));

                // Determine the actual quantity to be obtained.
                uint16_t ap_fetch_count = ap_total_count;
                if (ap_fetch_count > 5) {
                    ap_fetch_count = 5;
                }

                // malloc memory to store scan ap records
                wifi_ap_record_t *ap_records = (wifi_ap_record_t *)
                heap_caps_malloc(sizeof(wifi_ap_record_t) * ap_fetch_count, MALLOC_CAP_SPIRAM| MALLOC_CAP_8BIT);
                
                // gey ap records
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_fetch_count, ap_records));

                wifi_scan_done_data_t data = {.ap_count = ap_fetch_count, .ap_records = ap_records};
                

                ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle,APP_EVENT, APP_WIFI_SCAN_DONE, &data, sizeof(data), portMAX_DELAY));
                
                
            }
            // If current_wifi_state != wifi_state_busy, a "scan done" event will occur, but the WiFi has already been deinitialized at that point.
            // The fundamental reason is that wifi scan, wifi stop, and wifi start are all asynchronous processes; the function returning does not mean the operation has completed.
            // A contradictory situation will occur: during scanning, stop is called which deinitializes the WiFi, and then scan done is triggered afterward.
            // Calling esp_wifi_scan_get_ap_num at this time will cause a crash.
            else 
            {
                ESP_LOGW(TAG,"scan done meet illegal state %d",current_wifi_state);

            }
              
            break;

        //sta connected succesfully event
        case WIFI_EVENT_STA_CONNECTED:

            if (current_wifi_state == WIFI_STATE_BUSY) 
            {
                current_wifi_state = WIFI_STATE_CONNECTED;
                ESP_LOGI(TAG, "WiFi successfully connected, state -> CONNECTED");
                // Optional: post UI event notification success
                // esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_WIFI_CONNECT_SUCCESS, NULL, 0, portMAX_DELAY);
            }


            break;

        //sta  disconnect  event
        case WIFI_EVENT_STA_DISCONNECTED:

            // If the disconnection is triggered by esp wifi stop, then disconnect the WiFi and return to the closed state.
            if(current_wifi_state == WIFI_STATE_CLOSING)
            {
                // current_wifi_state=WIFI_STATE_CLOSED;

                // ESP_LOGI(TAG, "WiFi disconnected, state -> CLOSED");

            }
            else
            {
                // Abnormal disconnection scenarios (e.g., AP actively disconnecting / being out of range, etc.)
                uint8_t exception_disconnect_flag=1;

                esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_WIFI_DISCONNECTED, &exception_disconnect_flag, sizeof(uint8_t), portMAX_DELAY);
                ESP_LOGI(TAG, "WiFi disconnected, state -> IDLE");
                current_wifi_state=WIFI_STATE_IDLE;
            }                
            break;
        }
    }
    // Handle system WiFi-related events
    else if(event_base == IP_EVENT)
    {
        if(event_id==IP_EVENT_STA_GOT_IP)
        {
            if(current_wifi_state==WIFI_STATE_CONNECTED)
            {
                current_wifi_state=WIFI_STATE_GETIP;
                
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                
                // Successfully obtained IP address, event published.
                char ip_str[16]; // Store IP address string
                sprintf(ip_str, IPSTR, IP2STR(&event->ip_info.ip));
                
                ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle,APP_EVENT, APP_GET_IP, ip_str, strlen(ip_str)+1, portMAX_DELAY));

                // Automatic synchronization event
                ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle,APP_EVENT, APP_SNTP_REQUEST, NULL,0, portMAX_DELAY));

            }

        }
    }


    xSemaphoreGive(wifi_mutex);

}

/**
 * @brief UI event handler to start WiFi scanning or initialize WiFi.
 *
 * Responds to APP_WIFI_SCAN_START events posted by the UI. Depending on the
 * current WiFi state, this handler may initialize and start the WiFi stack,
 * begin an active scan, or ignore duplicate requests. State transitions are
 * protected by @c wifi_mutex.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base (APP_EVENT).
 * @param event_id Event identifier (APP_WIFI_SCAN_START).
 * @param event_data Event-specific data (unused).
 */
// User requests scan
static void  user_wifi_start_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    ESP_LOGI(TAG, "Received UI WIFI_START event");

    if (xSemaphoreTake(wifi_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Mutex take timeout in START handler");
        return ;
    }

    switch (current_wifi_state) 
    {
        case WIFI_STATE_CLOSED:

            // From CLOSED -> INITING: initialize and start
            current_wifi_state = WIFI_STATE_INITING;
            ESP_LOGI(TAG, "State CLOSED -> INITING");
            esp_netif_init();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            esp_wifi_init(&cfg);
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();  // Asynchronous: wait for WIFI_EVENT_STA_START after returning
            

            break;
        
        case WIFI_STATE_CLOSING:

             break;

        case WIFI_STATE_INITING:
            // Initializing, ignore or wait (blocking during rapid clicks has been handled)
            ESP_LOGW(TAG, "Already INITING, ignoring duplicate START");
            break;
        case WIFI_STATE_IDLE:
            // Already in IDLE, directly enter BUSY (e.g., scan)
            ESP_LOGI(TAG, "State IDLE -> BUSY (starting scan)");
            esp_wifi_scan_start(NULL, false);  // Non-blocking, BUSY state
            current_wifi_state = WIFI_STATE_BUSY;
            break;
        case WIFI_STATE_BUSY:
            // In the midst of busyness, overlooked
            ESP_LOGW(TAG, "WiFi BUSY, ignoring START");
            break;
        case WIFI_STATE_CONNECTED:

            break;
        
        case WIFI_STATE_GETIP:

            break;
        default :
        break;
        

    }

    xSemaphoreGive(wifi_mutex);
    
}





/**
 * @brief UI event handler to stop WiFi and deinitialize resources.
 *
 * Handles APP_WIFI_CLOSE events by transitioning state to CLOSING and
 * calling esp_wifi_stop() (asynchronous). The actual deinitialization is
 * completed in the WIFI_EVENT_STA_STOP handler in @c wifi_system_event_handler.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base (APP_EVENT).
 * @param event_id Event identifier (APP_WIFI_CLOSE).
 * @param event_data Event-specific data (unused).
 */
// User requests close
static void user_wifi_close_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    ESP_LOGI(TAG, "Received  WIFI_CLOSE event");

    if (xSemaphoreTake(wifi_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Mutex take timeout in CLOSE handler");
        return;
    }

    if (current_wifi_state == WIFI_STATE_CLOSED) 
    {
        ESP_LOGW(TAG, "Already CLOSED, ignoring CLOSE");
        xSemaphoreGive(wifi_mutex);
        return ;
    }

    // Transition from any active state to CLOSING
    current_wifi_state = WIFI_STATE_CLOSING;
    ESP_LOGI(TAG, "State -> CLOSING, calling esp_wifi_stop()");
    
    // esp_wifi_stop() is asynchronous; it will trigger the WIFI_EVENT_STA_STOP event.
    ESP_ERROR_CHECK(esp_wifi_stop());
    
    xSemaphoreGive(wifi_mutex);

    
    
}




/**
 * @brief UI event handler to initiate a WiFi station connection.
 *
 * Expects @p event_data to point to a wifi_connect_data_t containing SSID
 * and password. Configures the STA interface and calls esp_wifi_connect()
 * asynchronously. This handler is effective only when WiFi is in the
 * IDLE state; otherwise the request is ignored.
 *
 * @param arg Unused handler argument.
 * @param event_base Event base (APP_EVENT).
 * @param event_id Event identifier (APP_WIFI_CONNECT_REQUEST).
 * @param event_data Pointer to wifi_connect_data_t with connection info.
 */
// User requests connection
static void user_wifi_connect_request_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data) 
{
    if (xSemaphoreTake(wifi_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Mutex take timeout in CONNECT handler");
        return ;
    }

    if (current_wifi_state != WIFI_STATE_IDLE) 
    {
        ESP_LOGW(TAG, "WiFi not in IDLE state(now state is (%d)), ignoring connect", current_wifi_state);
        xSemaphoreGive(wifi_mutex);
        return ;
    }

    wifi_connect_data_t *connect_data = (wifi_connect_data_t*) event_data;
    ESP_LOGI(TAG, "Received connect request for SSID: %s", connect_data->ssid);

    // Configure WiFi STA (Station)
    wifi_config_t wifi_cfg = {0};
    strncpy((char*)wifi_cfg.sta.ssid, connect_data->ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, connect_data->password, sizeof(wifi_cfg.sta.password) - 1);

    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;  
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());  // Asynchronous connection

    current_wifi_state = WIFI_STATE_BUSY;  // Update status to BUSY (connecting)
    ESP_LOGI(TAG, "WiFi connect initiated, state -> BUSY");

    xSemaphoreGive(wifi_mutex);
    


}








