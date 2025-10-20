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

/**
 * @file camera.c
 * @brief Camera capture, rotation and display helpers.
 *
 * This module configures the camera peripheral, provides a double-buffered
 * pipeline to rotate frames (using esp_imgfx_rotate) and push them to the
 * LVGL display, and supports saving frames to BMP on the SD card.
 */

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

static const char * TAG="app_camera ";


// use double buffer
static uint8_t *frame_out_buf[2] = {NULL, NULL};  


// Input rotated pixel data
static esp_imgfx_data_t in_image;

//every frame buffer have a semaphore
static SemaphoreHandle_t frame_buf_sem[2];  

//queue
static QueueHandle_t    xQueueLCDFrame=NULL;  

QueueHandle_t    g_Camera_event_queue=NULL;  

//refresh camera frame to display
TaskHandle_t process_lcd_handle=NULL;

//get  camera frame task
TaskHandle_t process_camera_handle=NULL;

uint8_t first_save_camera_frame_flag=1;

//saved file name counter 
int  camera_frame_save_file_counter;


//
typedef struct 
{
    uint8_t *data;        // point to frame buffer
    uint8_t  buf_index;   // 0/1 indicate double buffer use
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



//init camera
esp_err_t  bsp_camera_init(void)
{
    camera_dvp_pwdn_set(0); // Power on camera module

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_1; // LEDC channel selection (used to generate XCLK on some chips); not required on ESP32-S3
    config.ledc_timer = LEDC_TIMER_1;     // LEDC timer selection (used to generate XCLK on some chips); not required on ESP32-S3
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
    config.pin_sccb_sda = -1;   // Use -1 to indicate the already-initialized I2C interface is used
    config.pin_sccb_scl = -1;
    config.sccb_i2c_port = EXAMPLE_I2C_NUM;       //
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
    esp_err_t err = esp_camera_init(&config); // Configure camera with the settings above
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get(); // Get camera sensor information

    if (s->id.PID == GC0308_PID) 
    {
        
        s->set_hmirror(s, 1);  // Set horizontal mirror: 1 = mirror, 0 = no mirror
    }

    return err;

}


/**
 * @brief Deinitialize the camera subsystem and free related resources.
 *
 * This function powers down the camera, stops and deletes camera and LCD
 * processing tasks, frees frame buffers and semaphores, deletes queues and
 * stores the last saved frame sequence number into NVS.
 */
void bsp_camera_deinit(void)
{
    camera_dvp_pwdn_set(1); // power down camera
    esp_camera_deinit();    

    //First, delete the task of generating frame data, then delete the refresh task
    vTaskDelete(process_camera_handle);
    vTaskDelete(process_lcd_handle);

    //free dynamic buffer
    heap_caps_free(frame_out_buf[0]);
    heap_caps_free(frame_out_buf[1]);
    frame_out_buf[0]=NULL;
    frame_out_buf[1]=NULL;

    heap_caps_free(in_image.data);
    in_image.data=NULL;


    //delete frame queue
    vQueueDelete(xQueueLCDFrame);
    vQueueDelete(g_Camera_event_queue);
    xQueueLCDFrame=NULL;
    g_Camera_event_queue=NULL;


    vSemaphoreDelete(frame_buf_sem[0]);
    vSemaphoreDelete(frame_buf_sem[1]);    

    frame_buf_sem[0]=NULL;
    frame_buf_sem[1]=NULL;


    //Save the current frame number
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

/**
 * @brief Set camera power-down GPIO level via IO extender.
 *
 * @param level 0 to power on the camera, non-zero to power it down.
 */
void camera_dvp_pwdn_set(uint8_t level)
{
    io_extend_set_level( 2, level);

}

/**
 * @brief LCD processing task.
 *
 * This task receives rotated frame buffers from the camera task via
 * `xQueueLCDFrame`, sets them as the LVGL image source and handles
 * save commands coming from `g_Camera_event_queue`. After display or
 * save operations it releases the corresponding buffer semaphore so the
 * camera task can reuse the buffer.
 *
 * @param arg Unused task parameter.
 */
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
                        //save current frame to sdcard
                        save_frame_to_bmp((const char *)buf_info.data);

                        break;
                    default:
                        break;

                }
            }
        
            //Release the semaphore after refreshing/saving the current frame 
            //to prevent race conditions
            if(xSemaphoreGive(frame_buf_sem[buf_info.buf_index])!=pdTRUE)
            {
                ESP_LOGE(TAG," give fram_buf_sem fail ");
            }
            // else 
                // ESP_LOGI(TAG," give fram_buf_sem success ");

            
        }
    }
}


/**
 * @brief Camera capture and rotate task.
 *
 * This task takes raw frames from `esp_camera_fb_get()`, copies them into
 * the rotation input buffer, performs a 90-degree rotation using the
 * esp_imgfx_rotate API into a double-buffered output, and sends the
 * rotated frame descriptor to the LCD queue (`xQueueLCDFrame`).
 * 
 * @note:
 * Since the screen is in portrait mode with a display size of 240 (width) * 320 (height), 
 * while the original camera frame obtained is in landscape mode 
 * with a size of 320 (width) * 240 (height).
 * 
 * The task uses semaphores (`frame_buf_sem`) to coordinate buffer usage
 * with the LCD task.
 *
 * @param arg Unused task parameter.
 */
static void task_process_camera(void *arg) 
{
    //configure swap arguments
    esp_imgfx_rotate_cfg_t cfg = {
        .in_pixel_fmt = ESP_IMGFX_PIXEL_FMT_RGB565_LE,
        .in_res = {.width = 320, .height = 240},
        .degree = 90,
    };

    esp_imgfx_rotate_handle_t handle;
    esp_imgfx_err_t ret = esp_imgfx_rotate_open(&cfg, &handle);
    assert(ESP_IMGFX_ERR_OK == ret);

    uint8_t  current_buf_index = 0;  //start index
    ESP_LOGI(TAG,"camera task create success");

    while (true) 
    {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) 
        {
            ESP_LOGE(TAG, "frame == NULL");
            continue;
        }

        //
        memcpy(in_image.data, frame->buf, 320 * 240 * 2);
        esp_camera_fb_return(frame);  //Release this frame early

        // ESP_LOGI(TAG,"take fram_buf_sem ing ");
        // wait a available semaphore which indicate a free frame buffer
        if (xSemaphoreTake(frame_buf_sem[current_buf_index], portMAX_DELAY) == pdTRUE) 
        {
            // ESP_LOGI(TAG,"take fram_buf_sem success ");
            esp_imgfx_data_t out_image = {.data_len = 240 * 320 * 2, .data = frame_out_buf[current_buf_index]};

            // swap 90 
            ret = esp_imgfx_rotate_process(handle, &in_image, &out_image);
            assert(ESP_IMGFX_ERR_OK == ret);

            // send to queue
            frame_buf_info_t buf_info = {.data = out_image.data, .buf_index = current_buf_index};
            xQueueSend(xQueueLCDFrame, &buf_info, portMAX_DELAY);

            //switch to next index 
            current_buf_index = (current_buf_index + 1) % 2;
        }
    }
    esp_imgfx_rotate_close(handle);
    vTaskDelete(NULL);
}


/**
 * @brief Start camera capture and LCD display tasks.
 *
 * This function initializes the double buffers and creates two FreeRTOS
 * tasks: one for capturing and rotating frames, and one for displaying
 * frames on the LVGL LCD.
 */
void app_camera_lcd(void)
{
    //init double buffer
    app_camera_double_buff_init();

    xTaskCreatePinnedToCore(task_process_camera, "task_process_camera", 3 * 1024, NULL, 5, &process_camera_handle, 1);
    xTaskCreatePinnedToCore(task_process_lcd, "task_process_lcd", 4 * 1024, NULL, 5, &process_lcd_handle, 0);

    ESP_LOGI(TAG,"create camera and lcd task success ");


}


/**
 * @brief Allocate double buffers, semaphores and queues used by camera pipeline.
 *
 * Allocates two output buffers in PSRAM, an input buffer for rotation,
 * creates binary semaphores for buffer handoff, and creates queues for
 * passing frames and camera commands.
 */
void app_camera_double_buff_init(void) 
{
    // Pre-allocated double buffering
    frame_out_buf[0] = heap_caps_aligned_alloc(128, 240 * 320 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    frame_out_buf[1] = heap_caps_aligned_alloc(128, 240 * 320 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(frame_out_buf[0] && frame_out_buf[1]);

    
    in_image.data=heap_caps_aligned_alloc(128, 240 * 320 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    in_image.data_len=320*240*2;
    assert(in_image.data);


    // Create a semaphore, initially available
    frame_buf_sem[0] = xSemaphoreCreateBinary();
    frame_buf_sem[1] = xSemaphoreCreateBinary();

    // Indicates that both buffers are available.
    xSemaphoreGive(frame_buf_sem[0]);
    xSemaphoreGive(frame_buf_sem[1]);

    // Create a queue (passing a struct)
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



/**
 * @brief Save a rotated RGB565 frame buffer to a 24-bit BMP file on SD card.
 *
 * The function converts the provided RGB565 buffer to RGB888 (BGR order
 * for BMP), writes a BITMAPINFOHEADER and pixel data, and increments a
 * saved-file counter stored in `camera_frame_save_file_counter`.
 *
 * @param out_image Pointer to the RGB565 frame buffer to save.
 */
void save_frame_to_bmp(const char* out_image)
{

    // First time sending a save frame request
    if(first_save_camera_frame_flag==1)
    {
        first_save_camera_frame_flag=0;

        // Check and create directory
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

    // Open the file, and if the file exists, clear its contents.
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

    // Convert RGB565 to RGB888 (BGR order for BMP)
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

        // BMP uses BGR order
        rgb888_buf[i * 3 + 0] = b8;
        rgb888_buf[i * 3 + 1] = g8;
        rgb888_buf[i * 3 + 2] = r8;
    }

    // BMP File Header (14 bytes)
    uint8_t bmp_header[14] = {
        'B', 'M',               // Signature
        0, 0, 0, 0,             // File size (to be filled in later)
        0, 0,                   // Keep
        0, 0,                   // Keep
        54, 0, 0, 0             // Data offset (14 + 40 = 54)
    };
    uint32_t file_size = 54 + rgb888_size;
    bmp_header[2] = (uint8_t)(file_size & 0xFF);
    bmp_header[3] = (uint8_t)((file_size >> 8) & 0xFF);
    bmp_header[4] = (uint8_t)((file_size >> 16) & 0xFF);
    bmp_header[5] = (uint8_t)((file_size >> 24) & 0xFF);

    // DIB Header (BITMAPINFOHEADER, 40 bytes)
    uint8_t dib_header[40] = {
        40, 0, 0, 0,            // Head size
        0, 0, 0, 0,             // Width (240)
        0, 0, 0, 0,             // Altitude (-320, top-down)
        1, 0,                   // Plane count (biPlanes=1)
        24, 0,                  // Bit depth (24 bpp)
        0, 0, 0, 0,             // Compression (BI_RGB=0)
        0, 0, 0, 0,             // Image size (rgb888_size, to be filled later)
        0, 0, 0, 0,             // X Resolution (0)
        0, 0, 0, 0,             // Y Resolution (0)
        0, 0, 0, 0,             // Number of palette colors (0)
        0, 0, 0, 0              // Number of important colors (0)
    };
    // Width 240
    dib_header[4] = (uint8_t)(width & 0xFF);
    dib_header[5] = (uint8_t)((width >> 8) & 0xFF);
    // Height -320 (top-down, avoid flipping pixel data)
    int32_t dib_height = -height;
    dib_header[8] = (uint8_t)(dib_height & 0xFF);
    dib_header[9] = (uint8_t)((dib_height >> 8) & 0xFF);
    dib_header[10] = (uint8_t)((dib_height >> 16) & 0xFF);
    dib_header[11] = (uint8_t)((dib_height >> 24) & 0xFF);
    // Image size
    uint32_t img_size = rgb888_size;
    dib_header[20] = (uint8_t)(img_size & 0xFF);
    dib_header[21] = (uint8_t)((img_size >> 8) & 0xFF);
    dib_header[22] = (uint8_t)((img_size >> 16) & 0xFF);
    dib_header[23] = (uint8_t)((img_size >> 24) & 0xFF);

    // Write header and data
    fwrite(bmp_header, 1, sizeof(bmp_header), f);
    fwrite(dib_header, 1, sizeof(dib_header), f);
    fwrite(rgb888_buf, 1, rgb888_size, f);

    // Release buffer
    heap_caps_free(rgb888_buf);

    fclose(f);
    ESP_LOGI(TAG, "Frame saved to %s", filename);
}



/**
 * @brief Register camera-related event handlers on the UI event loop.
 *
 * Registers handlers for application-level camera enter/exit events so
 * the camera can be initialized or deinitialized in response to UI actions.
 */
void camera_register_camera_handler(void)
{
    //init camera handler
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_CAMERA_ENTER, user_camera_init_handler, NULL));

    //deinit camera handler
    ESP_ERROR_CHECK(esp_event_handler_register_with(ui_event_loop_handle, APP_EVENT, APP_CAMERA_EXIT, user_camera_deinit_handler, NULL));

}


/**
 * @brief Handle APP_CAMERA_ENTER event: initialize camera and start display.
 *
 * Waits for any pending I2C transactions to finish, then initializes the
 * camera hardware and starts the camera/LCD tasks with `app_camera_lcd()`.
 */
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


/**
 * @brief Handle APP_CAMERA_EXIT event: deinitialize camera and free resources.
 *
 * This handler stops the camera and releases all resources by calling
 * `bsp_camera_deinit()`.
 */
static void user_camera_deinit_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG,"receive camera_exit event");
    
    bsp_camera_deinit();


}
