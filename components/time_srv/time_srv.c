#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"


#include "time_srv.h"
#include "event.h"


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static const char *TAG="sntp";


#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

static void user_sntp_srv_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

//1s发送一次sntp syned 事件，来改变相应widget
static void periodic_timer_callback(void* arg);

static void obtain_time(void);

void time_update_init(void);

void time_sync_notification_cb(struct timeval *tv);

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------





void time_register_sntp_handler(void)
{
    //注册sntp request handler
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_SNTP_REQUEST, user_sntp_srv_handler, NULL));


}



static void user_sntp_srv_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{

    time_t now;
    struct tm timeinfo;

    time(&now);

    if(sizeof(time_t)== 8)
    {
        ESP_LOGI("mytest"," now time is %"PRIu64,(uint64_t)now);
    }

    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");

        obtain_time();
    }

    char strftime_buf[64];

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();

    time(&now);
    localtime_r(&now, &timeinfo);

    //convert timeinfo to string 
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
    
    time_update_init();

    uint8_t retry_count=0;

    while(esp_event_post_to(ui_event_loop_handle,APP_EVENT, APP_SNTP_SYNED, NULL, 0, portMAX_DELAY)==ESP_ERR_TIMEOUT &&retry_count <5)
    {
        retry_count++;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    
}


static void obtain_time(void)
{

    ESP_LOGI(TAG, "Initializing and starting SNTP");

    /*
     * This is the basic default config with one server and starting the service
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want

    esp_netif_sntp_init(&config);

    


    int retry = 0;
    const int retry_count = 20;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) 
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }



    esp_netif_sntp_deinit();  
}



void time_update_init(void)
{
    const esp_timer_create_args_t second_timer = 
    {
        .callback = &periodic_timer_callback,

        //改变ui 秒数控件
        .name = "second_increment",
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&second_timer, &periodic_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000));


}


//1s发送一次sntp syned 事件，来改变相应widget
static void periodic_timer_callback(void* arg)
{

    esp_event_post_to(ui_event_loop_handle,APP_EVENT,APP_SNTP_SYNED,NULL,0,0);

}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG,"sntp syned successfully");


}