#pragma once

#include    "esp_err.h"


#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 5
#define CAMERA_PIN_SIOD 1
#define CAMERA_PIN_SIOC 2

#define CAMERA_PIN_D7 9
#define CAMERA_PIN_D6 4
#define CAMERA_PIN_D5 6
#define CAMERA_PIN_D4 15
#define CAMERA_PIN_D3 17
#define CAMERA_PIN_D2 8
#define CAMERA_PIN_D1 18
#define CAMERA_PIN_D0 16
#define CAMERA_PIN_VSYNC 3
#define CAMERA_PIN_HREF 46
#define CAMERA_PIN_PCLK 7

#define XCLK_FREQ_HZ 24000000


typedef enum {

    //保存当前帧
    CAMER_CMD_STORE,

} camera_command_type_t;

typedef struct 
{
    camera_command_type_t type;
    void *data; // 使用一个通用指针来传递数据
} camera_command_t;


extern QueueHandle_t    g_Camera_event_queue;  


esp_err_t bsp_camera_init(void);

void bsp_camera_deinit(void);

void app_camera_lcd(void);

void camera_dvp_pwdn_set(uint8_t level);


void save_frame_to_bmp(const char* out_image) ;


void camera_register_camera_handler(void);
