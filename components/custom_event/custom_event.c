#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "custom_event.h"
#include "driver/gpio.h"
#include "esp_sleep.h"


static const char *TAG="custom_event";

TimerHandle_t  auto_deep_sleep_timer;

//32bit, second 
TickType_t  auto_deep_sleep_timer_timeout_s;


void auto_deep_sleep_cb( TimerHandle_t xTimer );



/* UI_EVENT  definitions */
ESP_EVENT_DEFINE_BASE(APP_EVENT);

esp_event_loop_handle_t   ui_event_loop_handle;






/**
 * @brief Create and configure the UI event loop used for inter-component events.
 *
 * This function creates a dedicated FreeRTOS-based esp_event loop for
 * application UI events and stores the resulting handle in
 * `ui_event_loop_handle`. 
 * Other modules should post events to this loop to decouple event producers from
 * consumers.
 * 
 */
void  ui_event_loop_init(void)
{
    esp_event_loop_args_t loop_args = {
        .queue_size = 8,
        .task_name = "ui_event_loop",
        .task_priority = 10,    
        .task_stack_size = 5120,
        .task_core_id = 1
    };
    
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &ui_event_loop_handle));


}

//init auto sleep timer 
//if you click the screen, will reset the timer
//if you not click the screen exceed specific ,then will enter deep sleep mode
void auto_deep_sleep_init(void)
{

    ESP_LOGI(TAG,"auto sleep mode timer init");
    auto_deep_sleep_timer=xTimerCreate("deep sleep", pdMS_TO_TICKS(auto_deep_sleep_timer_timeout_s*1000), pdFALSE,
                                    NULL,auto_deep_sleep_cb);

    if(xTimerStart(auto_deep_sleep_timer,pdMS_TO_TICKS(5000))!=pdTRUE)
    {
        ESP_LOGE(TAG, "auto sleep timer start fial ");
        return ;
    }                              



}

void auto_deep_sleep_timer_reset(void)
{

    if(xTimerReset(auto_deep_sleep_timer,pdMS_TO_TICKS(5000))!=pdTRUE )
    {
        ESP_LOGE(TAG, "reset auto sleep timer fail ");
        return ;
    }   

}
uint32_t auto_sleep_time_get(void)
{
    return auto_deep_sleep_timer_timeout_s;
    
}

void auto_deep_sleep_timer_change(TickType_t period_s)
{

    auto_deep_sleep_timer_timeout_s=period_s;
    if(xTimerChangePeriod(auto_deep_sleep_timer,pdMS_TO_TICKS(period_s*1000), pdMS_TO_TICKS(5000))!=pdTRUE )
    {
        ESP_LOGE(TAG, "reset auto sleep timer fail ");
        return ;
    }   

}


void auto_deep_sleep_cb( TimerHandle_t xTimer )
{

    //configure gpio wake up ,when GPIO_NUM_O level=0 wake up whole system
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); 

    //enter deep sleep mode
    esp_deep_sleep_start();

}





