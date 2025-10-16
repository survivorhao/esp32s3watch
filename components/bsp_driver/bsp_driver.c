#include    <stdio.h>

#include    "esp_log.h"
#include    "driver/spi_master.h"
#include    "driver/ledc.h"

#include    "io_extend.h"
#include    "esp_err.h"
#include    "bsp_driver.h"
#include    "camera.h"


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

//definition

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


static const char* TAG="bsp_driver";
#define BSP_BUTTON_IO   GPIO_NUM_0


//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// declaration

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------


esp_err_t bsp_display_brightness_init(void);


//gpio interrupt service routine
static void IRAM_ATTR gpio_isr_handler(void* arg);



//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------

// gap

//----------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------











#if     I2C_DRIVER_VERSION==0
//----------------------------------------------------------------------------------------------------------
//-----------------------------------------i2c  旧 版 本 -----------------------------------------------------
//----------------------------------------------------------------------------------------------------------


/*
    @desc:  初始化i2c0驱动（只能适应旧版本，基于新版本的驱动的应用层开发不可调用）

*/
void    bsp_i2c_driver_init_legacy(void)
{
    ESP_LOGI(TAG, "Init I2C bus ");
    i2c_config_t i2c_conf = 
    {
        .sda_io_num = EXAMPLE_I2C_SDA_IO,
        .scl_io_num = EXAMPLE_I2C_SCL_IO,
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = EXAMPLE_I2C_BUS_FRE,            //总线频率 100k
    };
    ESP_ERROR_CHECK(i2c_param_config(EXAMPLE_I2C_NUM, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(EXAMPLE_I2C_NUM, i2c_conf.mode, 0, 0, 0));



}




#else
//----------------------------------------------------------------------------------------------------------
//----------------------------------------- i2c 新 版 本 -----------------------------------------------------
//----------------------------------------------------------------------------------------------------------

i2c_master_bus_handle_t     I2C_Master_bus_handle;
i2c_master_dev_handle_t     I2C_Master_oled_dev_handle;
i2c_master_dev_handle_t     I2C_Master_io_extend_dev_handle;

/*
    @desc: 初始化硬件i2c0 (只能适应新版本，基于旧版本的驱动的应用层开发不可调用)

*/
void   bsp_i2c_driver_init(void)
{

    //初始化i2c 总线
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = EXAMPLE_I2C_SDA_IO,
        .scl_io_num = EXAMPLE_I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &I2C_Master_bus_handle));


    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = (0x78>>1),                 //oled设备七位地址
        .scl_speed_hz = EXAMPLE_I2C_BUS_FRE,
    };
    
    // 未使用，注释即可
    //添加Oled设备
    // ESP_ERROR_CHECK(i2c_master_bus_add_device(I2C_Master_bus_handle, &dev_config, &I2C_Master_oled_dev_handle));


     //添加io extend 设备
    dev_config.device_address=(0x32>>1);             //io  extend 芯片七位地址
    ESP_ERROR_CHECK(i2c_master_bus_add_device(I2C_Master_bus_handle, &dev_config, &I2C_Master_io_extend_dev_handle));

}

#endif



//----------------------------------------------------------------------------------------------------------
//----------------------------------------- lcd相关 -----------------------------------------------------
//----------------------------------------------------------------------------------------------------------

// 定义液晶屏句柄
esp_lcd_panel_handle_t      lcd_panel_handle = NULL;    //tft lcd panel句柄
esp_lcd_panel_io_handle_t   lcd_io_handle = NULL;       //tft lcd spi 句柄
esp_lcd_touch_handle_t      tp_handle;                  // 触摸屏句柄



// 液晶屏初始化
esp_err_t bsp_display_new(void)
{
    esp_err_t ret = ESP_OK;
    // 背光初始化
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");
    // 初始化SPI总线
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,
        .mosi_io_num = BSP_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        // .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t),
        .max_transfer_sz = BSP_LCD_V_RES *20* sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");
    
    // 液晶屏控制IO初始化
    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 2,
        .trans_queue_depth = 5,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &lcd_io_handle), err, TAG, "New panel IO failed");
   
    // 初始化液晶屏驱动芯片ST7789
    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian=0,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(lcd_io_handle, &panel_config, &lcd_panel_handle), err, TAG, "New panel failed");
    
    esp_lcd_panel_reset(lcd_panel_handle);  // 液晶屏复位
    lcd_cs_level(0);  // 拉低CS引脚
    esp_lcd_panel_init(lcd_panel_handle);  // 初始化配置寄存器
    esp_lcd_panel_invert_color(lcd_panel_handle, true); // 颜色反转
    esp_lcd_panel_swap_xy(lcd_panel_handle, true);  // 显示翻转 
    esp_lcd_panel_mirror(lcd_panel_handle, true, false); // 镜像

    return ret;

err:
    if (lcd_panel_handle) {
        esp_lcd_panel_del(lcd_panel_handle);
    }
    if (lcd_io_handle) {
        esp_lcd_panel_io_del(lcd_io_handle);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}



// 背光PWM初始化
esp_err_t bsp_display_brightness_init(void)
{
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 0,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = true      //使用pmos管驱动lcd背光，pwm接到栅极因此需要Invert  ouput
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK

    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));

    return ESP_OK;
}

// 背光亮度设置
esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    } else if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    // ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);

    // LEDC resolution set to 10bits, thus: 100% = 1023
    uint32_t duty_cycle = (1023 * brightness_percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));

    return ESP_OK;
}

// 关闭背光
esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

// 打开背光 最亮
esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}


// 设置液晶屏颜色
void lcd_set_color(int x_start, int y_start, int x_end, int y_end, uint16_t rgb_color)
{
    //交换高低字节
    volatile uint16_t color_swap= ((((uint8_t)rgb_color)<<8)|((uint8_t)(rgb_color>>8)));
   
    // 分配内存 这里分配了液晶屏一行数据需要的大小
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(BSP_LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    if (NULL == buffer)
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
    }
    else
    {
        ESP_LOGI(TAG,"set screen back  color");
        
        for (size_t i = 0; i < (x_end -x_start); i++) // 给缓存中放入颜色数据
        {
            buffer[i] = color_swap;
        }
        for (int y = y_start; y < y_end; y++) // 显示整屏颜色
        {
            esp_lcd_panel_draw_bitmap(lcd_panel_handle, x_start, y, x_end, y+1, buffer);
        }
        free(buffer); // 释放内存
    }
}



void lcd_draw_pictrue(int x_start, int y_start, int x_end, int y_end, const unsigned char *gImage)
{
    
    ESP_LOGI(TAG,"begin draw test pic on  tft lcd ");
    size_t pixels_byte_size = (x_end - x_start)*(y_end - y_start) * 2;
    uint16_t *pixels = (uint16_t *)heap_caps_malloc((240 * 320) * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (NULL == pixels)
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }
    memcpy(pixels, gImage, pixels_byte_size);  // 把图片数据拷贝到内存

    esp_lcd_panel_draw_bitmap(lcd_panel_handle, x_start,y_start, x_end, y_end, (uint16_t *)pixels);
    heap_caps_free(pixels);

}







//----------------------------------------------------------------------------------------------------------
//----------------------------------------- lcd touch 相关 -----------------------------------------------------
//----------------------------------------------------------------------------------------------------------
esp_lcd_touch_handle_t      tp_handle;                  // 触摸屏句柄
esp_lcd_panel_io_handle_t   tp_io_handle;

void  bsp_touch_new(void)
{
    ESP_LOGI(TAG,"begin to init tft touch ");

    esp_lcd_panel_io_i2c_config_t   io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    io_config.scl_speed_hz=100000;

    // 4. 创建 Panel IO 句柄
    esp_lcd_new_panel_io_i2c(I2C_Master_bus_handle, &io_config, &tp_io_handle);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES,         //240
        .y_max = BSP_LCD_H_RES ,        //320
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .levels = 
        {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = 
        {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };

    esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp_handle);


}


void tft_touch_read_data(void* par)
{
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;
    bool touchpad_pressed ;
    ESP_LOGI(TAG,"touch detect task begin run !");
    while(1)
    {
        esp_lcd_touch_read_data(tp_handle);

        // 2. 从 tp 句柄中获取解析后的坐标数据
        touchpad_pressed = esp_lcd_touch_get_coordinates(tp_handle, touch_x, touch_y, touch_strength, &touch_cnt, 1);
        // 3. 检查是否有触摸点
        if (touchpad_pressed) 
        {
            ESP_LOGI(TAG, "Touch detected! x=%d, y=%d, strength=%d",
                     touch_x[0], touch_y[0], touch_strength[0]);
   
        } 

        // 4. 等待一段时间再次轮询
        vTaskDelay(pdMS_TO_TICKS(50)); // 例如每 50 毫秒轮询一次
    }
}


//----------------------------------------------------------------------------------------------------------
//-----------------------------------------display  and  touch  init 相关 -----------------------------------
//----------------------------------------------------------------------------------------------------------

lv_display_t *lvgl_disp= NULL;
static lv_indev_t *lvgl_touch_indev = NULL;


esp_err_t app_lvgl_init(void)
{
    //initialize lcd display , ledc (to control the lcd light), spi (to driver the lcd )
    bsp_display_new();
    
    //set  screen color white
    lcd_set_color(0,0,BSP_LCD_H_RES,BSP_LCD_V_RES,0xffff);

    //turn on  display
    esp_lcd_panel_disp_on_off(lcd_panel_handle,true);

    
    //open background light
    bsp_display_backlight_on();

    
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 15,         /* LVGL task priority */
        .task_stack = 6144,         /* LVGL task stack size */
        .task_affinity =0,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io_handle,
        .panel_handle = lcd_panel_handle,
        .buffer_size = 320 * 120,
        .double_buffer = 1,
        .hres = 320,
        .vres = 240,
        .monochrome = false,

        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false,   //渲染buffer分配在内部sram
            .buff_spiram=true,   //渲染buffer分配在psram
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    //initialize
    bsp_touch_new();

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = tp_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}





//----------------------------------------------------------------------------------------------------------
//-----------------------------------------按键检测 -----------------------------------
//----------------------------------------------------------------------------------------------------------




void bsp_button_init(void)
{
    gpio_config_t gpio_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,     // 下降沿中断
        .mode = GPIO_MODE_INPUT,            // 输入模式
        .pin_bit_mask = 1<<BSP_BUTTON_IO,   // 选择GPIO0
        .pull_down_en = 0,                 
        .pull_up_en = 1                     // 使能内部上拉
    };
    // 根据上面的配置 设置GPIO
    gpio_config(&gpio_conf);

    gpio_install_isr_service(0);

    
    // 给GPIO0添加中断处理
    gpio_isr_handler_add(BSP_BUTTON_IO, gpio_isr_handler, (void *)BSP_BUTTON_IO);

}



static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t io_num=(int)arg;
    if(io_num !=BSP_BUTTON_IO)
    {
        return ;
    }
    
    BaseType_t xHigherPriorityTaskWoken;

    camera_command_t cmd;
    cmd.type=CAMER_CMD_STORE;
    cmd.data=NULL;
    

    xQueueSendFromISR(g_Camera_event_queue, &cmd, &xHigherPriorityTaskWoken); // 把入口参数值发送到队列
    
    // if( xHigherPriorityTaskWoken )
    // {
    //     // Actual macro used here is port specific.
    //     portYIELD_FROM_ISR ();
    // }
    
}

