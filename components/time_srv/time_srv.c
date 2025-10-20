#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"
#include <stdint.h>
#include <inttypes.h>

#include "time_srv.h"
#include "event.h"


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static const char *TAG="time_srv";


//a variable stored in rtc ram  which indicate whether local time synchoronized or not 
//attention:  variables modified with RTC_NOINIT_ATTR could not be init when defination
//such as RTC_NOINIT_ATTR uint32_t rtc_local_time_syned_flag=1; this will not work 
RTC_NOINIT_ATTR uint32_t rtc_local_time_syned_flag;

//use this flag to init rtc_local_time_syned_flag =0 when power on reset
RTC_NOINIT_ATTR uint32_t rtc_magic;


#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

static void user_sntp_srv_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Send an SNTP synced event every 1 second to update the corresponding widget.
static void periodic_timer_callback(void* arg);

static void obtain_time(void);

void time_update_init(void);

void time_sync_notification_cb(struct timeval *tv);

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

/*
 * rtc_local_time_syned_flag_init
 * ------------------------------
 * Initialize the RTC-resident flag that indicates whether the local
 * time has been synchronized. This function uses a separate RTC magic
 * value to detect power-on resets; if the magic does not match the
 * expected value the flag is cleared and the magic is set.
 *
 * Inputs: none
 * Outputs: updates RTC_NOINIT_ATTR variables `rtc_local_time_syned_flag`
 *          and `rtc_magic` as a side-effect.
 * Notes: RTC_NOINIT_ATTR variables retain value across deep-sleep cycles
 *        when the magic matches; this routine ensures a clean init on
 *        power-on.
 */
void rtc_local_time_syned_flag_init(void)
{
    if(rtc_magic!=RTC_MAGIC_FLAG)
    {
        rtc_local_time_syned_flag=0;
        rtc_magic=RTC_MAGIC_FLAG;
        
    }
}



/*
 * time_register_sntp_handler
 * --------------------------
 * Register the local SNTP request event handler with the UI event loop.
 * This ties the application-specific `APP_SNTP_REQUEST` event to the
 * `user_sntp_srv_handler` callback so that when other parts of the
 * application post that event the handler will run.
 *
 * Inputs: none (uses global `ui_event_loop_handle` and event constants)
 * Outputs: registers an event handler; will abort on error via
 *          ESP_ERROR_CHECK.
 */
void time_register_sntp_handler(void)
{
    //register sntp request handler
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_SNTP_REQUEST, user_sntp_srv_handler, NULL));


}



/*
 * user_sntp_srv_handler
 * ---------------------
 * Event handler invoked in response to an `APP_SNTP_REQUEST` event.
 * The handler checks if the system time is already set; if not and the
 * RTC-synced flag indicates no prior sync, it will attempt to obtain
 * time from SNTP servers. After ensuring time is set, it posts an
 * `APP_SNTP_SYNED` event to the UI event loop so UI widgets can update.
 *
 * Parameters:
 *  - arg, event_base, event_id, event_data: standard esp_event handler args
 *
 * Side-effects:
 *  - May call `obtain_time()` which initializes SNTP and blocks until
 *    synchronization completes or times out.
 *  - Sets environment timezone to China Standard Time (CST-8).
 *  - Calls `time_update_init()` to start a 1-second periodic timer.
 *  - Posts `APP_SNTP_SYNED` to the UI event loop (with retries).
 */
static void user_sntp_srv_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{

    time_t now;
    struct tm timeinfo;

    time(&now);

    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG,"now rtc_local_time_syned_flag =%"PRIu32, rtc_local_time_syned_flag);


    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900) && (rtc_local_time_syned_flag==0)) 
    {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");

        //initialize sntp 
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



/*
 * obtain_time
 * -----------
 * Initialize SNTP (via esp-netif SNTP wrapper) and wait for time
 * synchronization. This function configures a default NTP server
 * ("pool.ntp.org"), registers a sync callback, and polls for
 * synchronization completion with a bounded retry loop.
 *
 * Inputs: none
 * Outputs / side-effects:
 *  - Starts SNTP and blocks while waiting for sync (with timeouts).
 *  - On success, sets `rtc_local_time_syned_flag = 1`.
 *  - Always deinitializes SNTP before returning.
 * Error handling:
 *  - Uses local retry counters and leaves `rtc_local_time_syned_flag` as
 *    0 on failure.
 */

static void obtain_time(void)
{

    ESP_LOGI(TAG, "Initializing and starting SNTP");

    /*
     * This is the basic default config with one server and starting the service
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    // Note: This is only needed if we want
    config.sync_cb = time_sync_notification_cb;     


    esp_netif_sntp_init(&config);

    
    int retry = 0;
    const int retry_count = 20;

    esp_err_t ret=ESP_ERR_TIMEOUT;
    while (ret== ESP_ERR_TIMEOUT && ++retry < retry_count) 
    {
        ret=esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000));
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }

    //success synchronize
    if(ret==ESP_OK)
    {
        //indicate local rtc timer synchronized success 
        rtc_local_time_syned_flag=1;
        
    }
    //try synchronize fail 
    else if(retry==retry_count)
    {
        
    }

    //free sntp resources 
    esp_netif_sntp_deinit();  
}


/*
 * time_update_init
 * ----------------
 * Create and start a periodic esp_timer that fires every 1 second.
 * The timer callback (`periodic_timer_callback`) posts a UI event
 * (`APP_SNTP_SYNED`) so widgets that display time can update once per
 * second.
 *
 * Inputs: none
 * Side-effects:
 *  - Allocates an esp_timer and starts it in periodic mode (1s period).
 *  - Aborts on timer creation/start failure via ESP_ERROR_CHECK.
 */

void time_update_init(void)
{
    const esp_timer_create_args_t second_timer = 
    {
        .callback = &periodic_timer_callback,

        //change second widget ui
        .name = "second_increment",
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&second_timer, &periodic_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000));


}



// Send an SNTP synced event every 1 second to update the corresponding widget.

/*
 * periodic_timer_callback
 * -----------------------
 * Timer callback executed every second by the esp_timer created in
 * `time_update_init`. Its sole job is to post an `APP_SNTP_SYNED` event
 * to the UI event loop (no blocking wait).
 *
 * Parameters:
 *  - arg: user-provided pointer from esp_timer (unused)
 * Side-effects: posts a UI event to trigger UI refresh of time widgets.
 */

static void periodic_timer_callback(void* arg)
{

    esp_event_post_to(ui_event_loop_handle,APP_EVENT,APP_SNTP_SYNED,NULL,0,0);

}


/*
 * time_sync_notification_cb
 * -------------------------
 * SNTP synchronization notification callback. This is registered with
 * the SNTP configuration so that the application can be notified when
 * the system time is successfully synchronized by SNTP.
 *
 * Parameters:
 *  - tv: pointer to the timeval representing the synchronized time
 * Side-effects: logs a message. The application may choose to extend
 *               this callback to perform additional actions on sync.
 */
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG,"sntp syned successfully");

}

