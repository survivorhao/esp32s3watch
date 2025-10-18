#include    <stdio.h>
#include    "esp_log.h"
#include    "esp_camera.h"
#include    "freertos/FreeRTOS.h"
#include    "freertos/task.h"
#include    "freertos/queue.h"
#include    "esp_imgfx_rotate.h"
#include    <sys/unistd.h>
#include    <sys/stat.h>
#include    <errno.h>
#include    <string.h>
#include    "esp_lvgl_port.h"
#include    "nvs_flash.h"


#include    "ui.h"
#include    "bsp_driver.h"
#include    "io_extend.h"
#include    "camera.h"
#include    "event.h"

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static const char * TAG="app_camera ";


// 全局变量
static uint8_t *frame_out_buf[2] = {NULL, NULL};  // 双缓冲


//输入旋转像素数据
static esp_imgfx_data_t in_image;

static SemaphoreHandle_t frame_buf_sem[2];  // 每个缓冲一个信号量

static QueueHandle_t    xQueueLCDFrame=NULL;  // 队列传 buf_info_t

QueueHandle_t    g_Camera_event_queue=NULL;  

//将camera frame 刷新到lcd任务
TaskHandle_t process_lcd_handle=NULL;

//读取camera frame任务
TaskHandle_t process_camera_handle=NULL;

uint8_t first_save_camera_frame_flag=1;

// 生成文件名（使用计数器递增）
int  camera_frame_save_file_counter;


// 定义缓冲结构体（用于队列传递）
typedef struct 
{
    uint8_t *data;  // 缓冲指针
    uint8_t  buf_index;  // 0 或 1
}frame_buf_info_t;





//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

void camera_dvp_pwdn_set(uint8_t level);
void app_camera_double_buff_init(void);

void save_frame_to_bmp(const char* out_image) ;

static void user_camera_init_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void user_camera_deinit_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------



// 摄像头硬件初始化
esp_err_t  bsp_camera_init(void)
{
    camera_dvp_pwdn_set(0); // 打开摄像头

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_1;  // LEDC通道选择  用于生成XCLK时钟 但是S3不用
    config.ledc_timer = LEDC_TIMER_1; // LEDC timer选择  用于生成XCLK时钟 但是S3不用
    config.pin_d0 = CAMERA_PIN_D0;
    config.pin_d1 = CAMERA_PIN_D1;
    config.pin_d2 = CAMERA_PIN_D2;
    config.pin_d3 = CAMERA_PIN_D3;
    config.pin_d4 = CAMERA_PIN_D4;
    config.pin_d5 = CAMERA_PIN_D5;
    config.pin_d6 = CAMERA_PIN_D6;
    config.pin_d7 = CAMERA_PIN_D7;
    config.pin_xclk = CAMERA_PIN_XCLK;
    config.pin_pclk = CAMERA_PIN_PCLK;              //pixel  clock 
    config.pin_vsync = CAMERA_PIN_VSYNC;
    config.pin_href = CAMERA_PIN_HREF;
    config.pin_sccb_sda = -1;                       // 这里写-1 表示使用已经初始化的I2C接口
    config.pin_sccb_scl = -1;
    config.sccb_i2c_port = 0;                      //修改了内部源码，确保deinit时不删除camera 的i2c device
    config.pin_pwdn = CAMERA_PIN_PWDN;
    config.pin_reset = CAMERA_PIN_RESET;
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.pixel_format = PIXFORMAT_RGB565;         //output  format 
    config.frame_size = FRAMESIZE_QVGA;             //output  frame  size  320*240
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;        //Frame buffer is placed in external PSRAM
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    // camera init
    esp_err_t err = esp_camera_init(&config); // 配置上面定义的参数
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get(); // 获取摄像头型号

    if (s->id.PID == GC0308_PID) 
    {
        
        s->set_hmirror(s, 1);  // 这里控制摄像头镜像 写1镜像 写0不镜像
    }

    return err;

}


void bsp_camera_deinit(void)
{
    camera_dvp_pwdn_set(1); // 关闭power线，关闭摄像头
    esp_camera_deinit();

    //先删除生成帧数据任务，再删除刷新任务
    vTaskDelete(process_camera_handle);
    vTaskDelete(process_lcd_handle);

    //释放buff
    heap_caps_free(frame_out_buf[0]);
    heap_caps_free(frame_out_buf[1]);
    frame_out_buf[0]=NULL;
    frame_out_buf[1]=NULL;

    heap_caps_free(in_image.data);
    in_image.data=NULL;


    //删除队列
    vQueueDelete(xQueueLCDFrame);
    vQueueDelete(g_Camera_event_queue);
    xQueueLCDFrame=NULL;
    g_Camera_event_queue=NULL;


    vSemaphoreDelete(frame_buf_sem[0]);
    vSemaphoreDelete(frame_buf_sem[1]);    

    frame_buf_sem[0]=NULL;
    frame_buf_sem[1]=NULL;


    //保存当前帧序号
    nvs_handle_t handle;
    esp_err_t err = nvs_open("user", NVS_READWRITE, &handle);
    if (err != ESP_OK) 
    {
       ESP_LOGE(TAG,"nvs open namespace fail ");
       return ;
    }
    nvs_set_i32(handle, "frame_seq", camera_frame_save_file_counter);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG,"camera application deinit success");




}


void camera_dvp_pwdn_set(uint8_t level)
{
    io_extend_set_level( 2, level);

}
static void task_process_lcd(void *arg) 
{

    camera_command_t    camera_cmd;
    frame_buf_info_t buf_info;
    ESP_LOGI(TAG,"lcd task create success ");
    lv_img_dsc_t  camera_img = {
    .header.always_zero = 0,
    .header.w = 240,
    .header.h = 320,
    .data_size = 320*240*2,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL
    };
    while (true) 
    {   
        // ESP_LOGI(TAG,"receive queuelcd frame ing ");
        if (xQueueReceive(xQueueLCDFrame, &buf_info, portMAX_DELAY)) 
        {
            // ESP_LOGI(TAG,"receive queuelcd frame success,sending .. ");
            // 绘制

            if(lvgl_port_lock(0)==true)
            {
                // ESP_LOGI(TAG,"lcd task :take lvgl mutex success");
                camera_img.data=buf_info.data;
                if(camera_display !=NULL)
                {
                    lv_img_set_src(camera_display, &camera_img);
                }
                lvgl_port_unlock();
            }
            else
            {
                 ESP_LOGE(TAG,"lcd task :take lvgl mutex  fail");
            }

            if(xQueueReceive(g_Camera_event_queue, &camera_cmd,0))
            {
                switch(camera_cmd.type)
                {
                    case CAMER_CMD_STORE:
 
                        ESP_LOGI(TAG,"receieve store command ");
                        //保存当前帧
                        save_frame_to_bmp((const char *)buf_info.data);

                        break;
                    default:
                        break;

                }
            }
        
            //刷新/保存当前帧之后再释放信号量，防止出现竞争情况
            if(xSemaphoreGive(frame_buf_sem[buf_info.buf_index])!=pdTRUE)
            {
                ESP_LOGE(TAG," give fram_buf_sem fail ");
            }
            // else 
                // ESP_LOGI(TAG," give fram_buf_sem success ");

            
        }
    }
}


static void task_process_camera(void *arg) 
{
    // 配置旋转参数（保持原样）
    esp_imgfx_rotate_cfg_t cfg = {
        .in_pixel_fmt = ESP_IMGFX_PIXEL_FMT_RGB565_LE,
        .in_res = {.width = 320, .height = 240},
        .degree = 90,
    };

    esp_imgfx_rotate_handle_t handle;
    esp_imgfx_err_t ret = esp_imgfx_rotate_open(&cfg, &handle);
    assert(ESP_IMGFX_ERR_OK == ret);

    uint8_t  current_buf_index = 0;  // 起始索引
    ESP_LOGI(TAG,"camera task create success");

    while (true) 
    {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) 
        {
            ESP_LOGE(TAG, "frame == NULL");
            continue;
        }

        // memcpy 到输入
        memcpy(in_image.data, frame->buf, 320 * 240 * 2);
        esp_camera_fb_return(frame);  // 早释放原帧

        // ESP_LOGI(TAG,"take fram_buf_sem ing ");
        // 等待一个可用缓冲（take 信号量）
        if (xSemaphoreTake(frame_buf_sem[current_buf_index], portMAX_DELAY) == pdTRUE) 
        {
            // ESP_LOGI(TAG,"take fram_buf_sem success ");
            esp_imgfx_data_t out_image = {.data_len = 240 * 320 * 2, .data = frame_out_buf[current_buf_index]};

            // 旋转
            ret = esp_imgfx_rotate_process(handle, &in_image, &out_image);
            assert(ESP_IMGFX_ERR_OK == ret);

            // 发送结构体到队列
            frame_buf_info_t buf_info = {.data = out_image.data, .buf_index = current_buf_index};
            xQueueSend(xQueueLCDFrame, &buf_info, portMAX_DELAY);

            // 切换下一个缓冲索引
            current_buf_index = (current_buf_index + 1) % 2;
        }
    }
    esp_imgfx_rotate_close(handle);
    vTaskDelete(NULL);
}

// 让摄像头显示到LCD
void app_camera_lcd(void)
{
    app_camera_double_buff_init();
    

    xTaskCreatePinnedToCore(task_process_camera, "task_process_camera", 3 * 1024, NULL, 5, &process_camera_handle, 1);
    xTaskCreatePinnedToCore(task_process_lcd, "task_process_lcd", 4 * 1024, NULL, 5, &process_lcd_handle, 0);

    ESP_LOGI(TAG,"create camera and lcd task success ");

}


// 在 app_main 或初始化函数中
void app_camera_double_buff_init(void) 
{
    // 预分配双缓冲
    frame_out_buf[0] = heap_caps_aligned_alloc(128, 240 * 320 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    frame_out_buf[1] = heap_caps_aligned_alloc(128, 240 * 320 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(frame_out_buf[0] && frame_out_buf[1]);

    
    in_image.data=heap_caps_aligned_alloc(128, 240 * 320 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    in_image.data_len=320*240*2;
    assert(in_image.data);


    // 创建信号量，初始可用
    frame_buf_sem[0] = xSemaphoreCreateBinary();
    frame_buf_sem[1] = xSemaphoreCreateBinary();

    //表明两个buf均可用
    xSemaphoreGive(frame_buf_sem[0]);
    xSemaphoreGive(frame_buf_sem[1]);

    // 创建队列（传结构体）
    xQueueLCDFrame = xQueueCreate(2, sizeof(frame_buf_info_t));
    if(!xQueueLCDFrame)
    {
        ESP_LOGE(TAG,"create lcd fram queue fail ");

    }

    g_Camera_event_queue= xQueueCreate(4,sizeof(camera_command_t));


    if(!g_Camera_event_queue)
    {
        ESP_LOGE(TAG,"create camera command queue fail");

    }

    
}




// 新增: 保存函数（将旋转后的 RGB565 转换为 RGB888 并保存到 BMP 文件）
// void save_frame_to_bmp(const esp_imgfx_data_t* out_image) 
void save_frame_to_bmp(const char* out_image)
{


    //第一次发送保存帧请求
    if(first_save_camera_frame_flag==1)
    {
        first_save_camera_frame_flag=0;

        // 检查并创建目录
        struct stat st;
        if (stat(CAMERA_SAVED_PIC_PATH, &st) != 0) 
        {
            if (mkdir(CAMERA_SAVED_PIC_PATH, 0777) != 0) 
            {
                ESP_LOGE(TAG, "Failed to create directory /sdcard/camera");
                return;
            }
        }

    }

    char filename[64];
    snprintf(filename, sizeof(filename), "/sdcard/camera/fra_%04d.bmp", camera_frame_save_file_counter++);

    // 打开文件,如果存在文件则清空
    FILE* f = fopen(filename, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file %s", filename);
        return;
    }

    const int width = 240;
    const int height = 320;
    const size_t rgb888_size = width * height * 3;
    uint8_t* rgb888_buf = (uint8_t*)heap_caps_malloc(rgb888_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rgb888_buf) {
        ESP_LOGE(TAG, "Failed to allocate RGB888 buffer");
        fclose(f);
        return;
    }

    // 转换 RGB565 到 RGB888 (BGR 顺序 for BMP)
    // uint16_t* rgb565_data = (uint16_t*)out_image->data;
    uint16_t* rgb565_data = (uint16_t*)out_image;
    for (int i = 0; i < width * height; i++) 
    {
        uint16_t pixel = rgb565_data[i];
        pixel=(uint16_t ) ((((uint8_t)pixel)<<8)|((uint8_t)(pixel>>8)));

        uint8_t r5 = (pixel >> 11) & 0x1F;
        uint8_t g6 = (pixel >> 5) & 0x3F;
        uint8_t b5 = pixel & 0x1F;

        uint8_t r8 = (r5 << 3) | (r5 >> 2);
        uint8_t g8 = (g6 << 2) | (g6 >> 4);
        uint8_t b8 = (b5 << 3) | (b5 >> 2);

        // BMP 使用 BGR 顺序
        rgb888_buf[i * 3 + 0] = b8;
        rgb888_buf[i * 3 + 1] = g8;
        rgb888_buf[i * 3 + 2] = r8;
    }

    // BMP 文件头 (14 bytes)
    uint8_t bmp_header[14] = {
        'B', 'M',               // 签名
        0, 0, 0, 0,             // 文件大小 (稍后填充)
        0, 0,                   // 保留
        0, 0,                   // 保留
        54, 0, 0, 0             // 数据偏移 (14 + 40 = 54)
    };
    uint32_t file_size = 54 + rgb888_size;
    bmp_header[2] = (uint8_t)(file_size & 0xFF);
    bmp_header[3] = (uint8_t)((file_size >> 8) & 0xFF);
    bmp_header[4] = (uint8_t)((file_size >> 16) & 0xFF);
    bmp_header[5] = (uint8_t)((file_size >> 24) & 0xFF);

    // DIB 头 (BITMAPINFOHEADER, 40 bytes)
    uint8_t dib_header[40] = {
        40, 0, 0, 0,            // 头大小
        0, 0, 0, 0,             // 宽度 (240)
        0, 0, 0, 0,             // 高度 (-320, top-down)
        1, 0,                   // 平面数 (biPlanes=1)
        24, 0,                  // 位深度 (24 bpp)
        0, 0, 0, 0,             // 压缩 (BI_RGB=0)
        0, 0, 0, 0,             // 图像大小 (rgb888_size, 稍后填充)
        0, 0, 0, 0,             // X 分辨率 (0)
        0, 0, 0, 0,             // Y 分辨率 (0)
        0, 0, 0, 0,             // 调色板颜色数 (0)
        0, 0, 0, 0              // 重要颜色数 (0)
    };
    // 宽度 240
    dib_header[4] = (uint8_t)(width & 0xFF);
    dib_header[5] = (uint8_t)((width >> 8) & 0xFF);
    // 高度 -320 (top-down，避免翻转像素数据)
    int32_t dib_height = -height;
    dib_header[8] = (uint8_t)(dib_height & 0xFF);
    dib_header[9] = (uint8_t)((dib_height >> 8) & 0xFF);
    dib_header[10] = (uint8_t)((dib_height >> 16) & 0xFF);
    dib_header[11] = (uint8_t)((dib_height >> 24) & 0xFF);
    // 图像大小
    uint32_t img_size = rgb888_size;
    dib_header[20] = (uint8_t)(img_size & 0xFF);
    dib_header[21] = (uint8_t)((img_size >> 8) & 0xFF);
    dib_header[22] = (uint8_t)((img_size >> 16) & 0xFF);
    dib_header[23] = (uint8_t)((img_size >> 24) & 0xFF);

    // 写入头和数据
    fwrite(bmp_header, 1, sizeof(bmp_header), f);
    fwrite(dib_header, 1, sizeof(dib_header), f);
    fwrite(rgb888_buf, 1, rgb888_size, f);

    // 释放缓冲
    heap_caps_free(rgb888_buf);

    fclose(f);
    ESP_LOGI(TAG, "Frame saved to %s", filename);
}




void camera_register_camera_handler(void)
{
    //init camera handler
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_CAMERA_ENTER, user_camera_init_handler, NULL));

    //deinit camera handler
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_CAMERA_EXIT, user_camera_deinit_handler, NULL));



}


static void user_camera_init_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{

    ESP_LOGI(TAG,"receive app_camera_enter event");
    if(i2c_master_bus_wait_all_done(I2C_Master_bus_handle,-1)==ESP_OK)
    {
        // i2c_master_bus_reset(I2C_Master_bus_handle);

        ESP_LOGI(TAG,"i2c bus transaction flush success");

        // vTaskSuspend();


    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    if(bsp_camera_init() == ESP_OK)
    {

        app_camera_lcd();
        
    }
    else
    {
        ESP_LOGE(TAG,"camera init fail, please check ");
    }


}


static void user_camera_deinit_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG,"receive camera_exit event");
    
    bsp_camera_deinit();



}