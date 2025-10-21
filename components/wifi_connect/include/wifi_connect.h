#pragma once


#include "sdkconfig.h"
#include "esp_err.h"




// WiFi Status Enumeration
typedef enum {
    WIFI_STATE_CLOSED=0x00,     // Close: All resources have been deinitialized
    WIFI_STATE_CLOSING,         // Closing,  no API calls can be made at this time.
    WIFI_STATE_INITING,         // Initializing:  esp_wifi_start() has been called, but the START event has not been received yet.
    WIFI_STATE_IDLE,            // Idle:  Initialization complete, ready to call scan/connect functions.
    WIFI_STATE_BUSY,            // Busy:  currently scanning or connecting
    WIFI_STATE_CONNECTED,       // Successfully connected to WiFi
    WIFI_STATE_GETIP,           // Successfully obtained IP address

} wifi_state_t;


extern  wifi_state_t  current_wifi_state;

// Mutex protects the state and WiFi operations to 
//ensure reliable changes to current_wifi_state.
extern SemaphoreHandle_t wifi_mutex ;         


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
void wifi_task(void *Par);





