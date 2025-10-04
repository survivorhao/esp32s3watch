idf_component_register(SRCS "bsp_driver.c"
                    INCLUDE_DIRS "include"
                    
                    PRIV_REQUIRES   
                    REQUIRES      freertos  driver  esp_lcd 
                    esp_lcd_touch   esp_lcd_touch_ft5x06  io_extend
                    lvgl   esp_lvgl_port camera
                    )
