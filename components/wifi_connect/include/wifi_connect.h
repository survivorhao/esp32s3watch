#pragma once


#include "sdkconfig.h"
#include "esp_err.h"




// WiFi 状态枚举
typedef enum {
    WIFI_STATE_CLOSED=0x00,     // 关闭：所有资源已 deinit
    WIFI_STATE_CLOSING,         //关闭中，此时不可调用任何api
    WIFI_STATE_INITING,         // 正在初始化：esp_wifi_start() 已调用，但未收到 START 事件
    WIFI_STATE_IDLE,            // 空闲：已初始化完成，可以调用 scan/connect
    WIFI_STATE_BUSY,            // 忙碌：正在 scan 或 connect
    WIFI_STATE_CONNECTED,       //成功连接wifi
    WIFI_STATE_GETIP,           //成功get ip address

    WIFI_STATE_HTTP_CLENT,      //发起http client 过程



} wifi_state_t;


extern  wifi_state_t  current_wifi_state;
extern SemaphoreHandle_t wifi_mutex ;          // 互斥量保护状态和 WiFi 操作,确保current_wifi_state的改变可靠


void wifi_task(void *Par);





