#pragma once


#include "sdkconfig.h"
#include "esp_err.h"




// WiFi Status Enumeration
typedef enum {
    WIFI_STATE_CLOSED=0x00,     // Close: All resources have been deinitialized
    WIFI_STATE_CLOSING,         // Closing, no API calls can be made at this time.
    WIFI_STATE_INITING,         // Initializing: esp_wifi_start() has been called, but the START event has not been received yet.
    WIFI_STATE_IDLE,            // Idle: Initialization complete, ready to call scan/connect functions.
    WIFI_STATE_BUSY,            // Busy: currently scanning or connecting
    WIFI_STATE_CONNECTED,       // Successfully connected to WiFi
    WIFI_STATE_GETIP,           // Successfully obtained IP address

    WIFI_STATE_HTTP_CLENT,      // The process of initiating an HTTP client



} wifi_state_t;


extern  wifi_state_t  current_wifi_state;
extern SemaphoreHandle_t wifi_mutex ;          // Mutex protects the state and WiFi operations to ensure reliable changes to current_wifi_state.


void wifi_task(void *Par);





