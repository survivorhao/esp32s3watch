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



// 全局变量
wifi_state_t current_wifi_state = WIFI_STATE_CLOSED;
SemaphoreHandle_t wifi_mutex = NULL;          // 互斥量保护状态和 WiFi 操作,确保current_wifi_state的改变可靠

static esp_event_handler_instance_t wifi_event_instance;    // WiFi 系统事件实例
static esp_event_handler_instance_t ip_event_instance;      // IP 事件实例


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static void wifi_system_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);


static void user_wifi_close_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// 用户请求扫描
static void user_wifi_start_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// 用户请求连接
static void user_wifi_connect_request_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


/// @brief wifi task 
/// @param pvParameters 
void wifi_task(void *Par) 
{
    // 初始化 mutex
    wifi_mutex = xSemaphoreCreateMutex();
    if (wifi_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi mutex");
        vTaskDelete(NULL);
        return;
    }

    // 创建默认事件循环（WiFi 系统事件）
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 注册 WiFi 系统事件 handler（处理异步事件如 START, STOP 等）
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_system_event_handler, NULL, &wifi_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_system_event_handler, NULL, &ip_event_instance));

    // APP_WIFI_SCAN_START
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_SCAN_START, user_wifi_start_handler, NULL));
    
    // APP_WIFI_CONNECT_REQUEST
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_CONNECT_REQUEST, user_wifi_connect_request_handler, NULL));

    // APP_WIFI_CLOSE
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_CLOSE, user_wifi_close_handler, NULL));





    // 清理（理论上不会到达）
    vTaskDelete(NULL);
}




/// @brief Unregister  relevant  handler 
/// @param NULL
void wifi_unregister_handler(void) 
{
    // 注销事件等
    // esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_instance);
    // esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_instance);
    // esp_event_handler_unregister_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_SCAN_START, user_wifi_start_handler);
    // esp_event_handler_unregister_with(ui_event_loop_handle, APP_EVENT, APP_WIFI_CLOSE, user_wifi_close_handler);
    // vSemaphoreDelete(wifi_mutex);

}










/// @brief  处理default event loop派发的相关wifi事件
/// @param arg 
/// @param event_base 
/// @param event_id 
/// @param event_data 
static void wifi_system_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) 
{
    if (xSemaphoreTake(wifi_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) 
    {
        ESP_LOGE(TAG, "Mutex take timeout in START handler");
        return ;

    }

    //处理系统wifi 相关事件
    if (event_base == WIFI_EVENT ) 
    {
       switch (event_id) 
       {
        //esp_wifi_start  成功
        case WIFI_EVENT_STA_START:

            if(current_wifi_state == WIFI_STATE_INITING) 
            {
                current_wifi_state = WIFI_STATE_IDLE;
                ESP_LOGI(TAG, "WiFi started successfully, state -> IDLE");
                
                // 可选：如果 START 事件意图是 scan，这里可以自动触发 scan
                ESP_LOGI(TAG,"wifi scan begin,idle -> busy");
                current_wifi_state = WIFI_STATE_BUSY;
                esp_wifi_scan_start(NULL, false);  // 非阻塞

                
            }
            else
            {
                ESP_LOGE(TAG,"illegal state =%d",current_wifi_state);

            }

    
            
            break;
        
        //sta stop event
        case WIFI_EVENT_STA_STOP:
            
            // esp_wifi_stop() 成功，允许 deinit
            if (current_wifi_state == WIFI_STATE_CLOSING) 
            {
                ESP_LOGI(TAG, "WiFi stopped, proceeding to deinit");
                esp_wifi_restore();  // 重置配置
                esp_wifi_deinit();
                esp_netif_deinit();
                current_wifi_state = WIFI_STATE_CLOSED;
                ESP_LOGI(TAG, "WiFi deinit complete, state -> CLOSED");

                //unregister wifi handler 
                wifi_unregister_handler();

            }
            break;    
        
        //sta scan done event 
        case WIFI_EVENT_SCAN_DONE:

            if (current_wifi_state == WIFI_STATE_BUSY) 
            {
                current_wifi_state = WIFI_STATE_IDLE;
                ESP_LOGI(TAG, "Scan done, state -> IDLE");
                
                // 获取扫描到的 AP 总数
                uint16_t ap_total_count;
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_total_count));

                // 决定实际要获取的数量
                uint16_t ap_fetch_count = ap_total_count;
                if (ap_fetch_count > 4) {
                    ap_fetch_count = 4;
                }

                // 根据你打算获取的数量来分配内存
                wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc( sizeof(wifi_ap_record_t) * ap_fetch_count);

                // 调用函数，传入你分配的内存大小，并让它告诉你实际获取的数量
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_fetch_count, ap_records));

                wifi_scan_done_data_t data = {.ap_count = ap_fetch_count, .ap_records = ap_records};
                

                ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle,APP_EVENT, APP_WIFI_SCAN_DONE, &data, sizeof(data), portMAX_DELAY));
                
                
            }
            //如果current_wifi_state != wifi_state_busy 就会出现scan done了，但是当前已经wifi 已经被deinit 的状态
            //本质原因是 wifi scan , wifi stop, wifi start都是异步过程，函数返回了不代表操作完成了
            //会出现一种矛盾状态，scaning 中调用了stop 把WiFi deinit了，之后scan done了
            //此时要是调用esp_wifi_scan_get_ap_num就会崩溃
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
                // 可选：post UI 事件通知成功
                // esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_WIFI_CONNECT_SUCCESS, NULL, 0, portMAX_DELAY);
            }


            break;

        //sta  disconnect  event
        case WIFI_EVENT_STA_DISCONNECTED:

            // 如果是esp wifi stop 触发的断开WiFi连接回到closed
            if(current_wifi_state == WIFI_STATE_CLOSING)
            {
                // current_wifi_state=WIFI_STATE_CLOSED;

                // ESP_LOGI(TAG, "WiFi disconnected, state -> CLOSED");

            }
            else
            {
                //异常断开连接情况（ap主动关闭/距离太远等事件）
                uint8_t exception_disconnect_flag=1;

                esp_event_post_to(ui_event_loop_handle, APP_EVENT, APP_WIFI_DISCONNECTED, &exception_disconnect_flag, sizeof(uint8_t), portMAX_DELAY);
                ESP_LOGI(TAG, "WiFi disconnected, state -> IDLE");
                current_wifi_state=WIFI_STATE_IDLE;
            }                
            break;
        }
    }
    //处理系统wifi 相关事件
    else if(event_base == IP_EVENT)
    {
        if(event_id==IP_EVENT_STA_GOT_IP)
        {
            if(current_wifi_state==WIFI_STATE_CONNECTED)
            {
                current_wifi_state=WIFI_STATE_GETIP;
                
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                
                // 成功获取IP地址，发布事件
                char ip_str[16]; // 存储IP地址字符串
                sprintf(ip_str, IPSTR, IP2STR(&event->ip_info.ip));
                
                ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle,APP_EVENT, APP_GET_IP, ip_str, strlen(ip_str)+1, portMAX_DELAY));

                //自动同步事件
                ESP_ERROR_CHECK(esp_event_post_to(ui_event_loop_handle,APP_EVENT, APP_SNTP_REQUEST, NULL,0, portMAX_DELAY));

            }

        }
    }


    xSemaphoreGive(wifi_mutex);

}

// 用户请求扫描
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

            // 从 CLOSED -> INITING：初始化并 start
            current_wifi_state = WIFI_STATE_INITING;
            ESP_LOGI(TAG, "State CLOSED -> INITING");
            esp_netif_init();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            esp_wifi_init(&cfg);
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();  // 异步：返回后等 WIFI_EVENT_STA_START
            

            break;
        
        case WIFI_STATE_CLOSING:

             break;

        case WIFI_STATE_INITING:
            // 正在 initing，忽略或等待（快速点击时阻塞已处理）
            ESP_LOGW(TAG, "Already INITING, ignoring duplicate START");
            break;
        case WIFI_STATE_IDLE:
            // 已 IDLE，直接进入 BUSY（如 scan）
            ESP_LOGI(TAG, "State IDLE -> BUSY (starting scan)");
            esp_wifi_scan_start(NULL, false);  // 非阻塞，BUSY 状态
            current_wifi_state = WIFI_STATE_BUSY;
            break;
        case WIFI_STATE_BUSY:
            // 忙碌中，忽略
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





/// @brief 关闭wifi 释放相应资源
/// @param arg 
/// @param event_base 
/// @param event_id 
/// @param event_data 
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

    // 从任何活跃状态转换到 CLOSING
    current_wifi_state = WIFI_STATE_CLOSING;
    ESP_LOGI(TAG, "State -> CLOSING, calling esp_wifi_stop()");
    
    // esp_wifi_stop() 是异步的，它将触发 WIFI_EVENT_STA_STOP
    ESP_ERROR_CHECK(esp_wifi_stop());
    
    xSemaphoreGive(wifi_mutex);

    
    
}




// 用户请求连接
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

    // 配置 WiFi STA
    wifi_config_t wifi_cfg = {0};
    strncpy((char*)wifi_cfg.sta.ssid, connect_data->ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, connect_data->password, sizeof(wifi_cfg.sta.password) - 1);

    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;  
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());  // 异步连接

    current_wifi_state = WIFI_STATE_BUSY;  // 更新状态到 BUSY (connecting)
    ESP_LOGI(TAG, "WiFi connect initiated, state -> BUSY");

    xSemaphoreGive(wifi_mutex);
    


}








