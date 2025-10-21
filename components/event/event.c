#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"




#include "event.h"




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










