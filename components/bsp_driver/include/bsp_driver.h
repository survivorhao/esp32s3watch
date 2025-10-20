#pragma  once

#include    "esp_lcd_types.h"
#include    "freertos/FreeRTOS.h"
#include    "freertos/task.h"
#include    "esp_lcd_panel_io.h"
#include    "esp_lcd_panel_vendor.h"
#include    "esp_lcd_panel_ops.h"
#include    "esp_lcd_touch_ft5x06.h"
#include    "esp_err.h"
#include    "esp_log.h"
#include    "esp_check.h"
#include    "lvgl.h"
#include    "esp_lvgl_port.h"
#include    "esp_lcd_touch.h"
#include    "driver/i2c_master.h"



extern  esp_lcd_panel_handle_t      lcd_panel_handle ;    // tft lcd panel handle
extern  esp_lcd_panel_io_handle_t   lcd_io_handle ;       // TFT LCD SPI handle
extern  esp_lcd_touch_handle_t      tp_handle;            // Touchscreen handle



extern i2c_master_bus_handle_t     I2C_Master_bus_handle;
extern i2c_master_dev_handle_t     I2C_Master_io_extend_dev_handle;




/* I2C port and GPIOs */
#define     EXAMPLE_I2C_NUM            I2C_NUM_0
#define     EXAMPLE_I2C_SDA_IO         GPIO_NUM_1
#define     EXAMPLE_I2C_SCL_IO         GPIO_NUM_2
#define     EXAMPLE_I2C_BUS_FRE        100000


void    bsp_i2c_driver_init(void);    







//----------------------------------------------------------------------------------------------------------
//------------------------------------tft  lcd  display  -----------------------------------------------------
//----------------------------------------------------------------------------------------------------------
#define BSP_LCD_PIXEL_CLOCK_HZ     (80 * 1000 * 1000)
#define BSP_LCD_SPI_NUM            (SPI3_HOST)
#define LCD_CMD_BITS               (8)
#define LCD_PARAM_BITS             (8)
#define BSP_LCD_BITS_PER_PIXEL     (16)
#define LCD_LEDC_CH                 LEDC_CHANNEL_0

#define BSP_LCD_H_RES              (320)
#define BSP_LCD_V_RES              (240)

#define BSP_LCD_SPI_MOSI      (GPIO_NUM_40)
#define BSP_LCD_SPI_CLK       (GPIO_NUM_41)
#define BSP_LCD_SPI_CS        (GPIO_NUM_NC)
#define BSP_LCD_DC            (GPIO_NUM_39)
#define BSP_LCD_RST           (GPIO_NUM_NC)
#define BSP_LCD_BACKLIGHT     (GPIO_NUM_42)  

#define BSP_LCD_DRAW_BUF_HEIGHT    (20)



esp_err_t bsp_display_brightness_init(void);
esp_err_t bsp_display_brightness_set(int brightness_percent);
esp_err_t bsp_display_backlight_off(void);
esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_display_new(void);
void lcd_draw_pictrue(int x_start, int y_start, int x_end, int y_end, const unsigned char *gImage);


void lcd_set_color(int x_start, int y_start, int x_end, int y_end, uint16_t rgb_color);

extern esp_lcd_panel_handle_t      lcd_panel_handle;    // tft lcd panel handle

//----------------------------------------------------------------------------------------------------------
// ----------------------------------------- lcd touch related -----------------------------------------------------
//----------------------------------------------------------------------------------------------------------
extern esp_lcd_touch_handle_t      tp_handle;                  // Touchscreen handle

void  bsp_touch_new(void);
void tft_touch_read_data(void* par);



//----------------------------------------------------------------------------------------------------------
// ----------------------------------------- Display and Touch Initialization Related -----------------------------------------------------
//----------------------------------------------------------------------------------------------------------

extern lv_display_t *lvgl_disp;

esp_err_t app_lvgl_init(void);




void bsp_button_init(void);
