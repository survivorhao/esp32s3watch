idf_component_register(SRCS "sdcard.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES sdmmc fatfs    
                    REQUIRES  esp_lvgl_port bsp_driver event ui
                    )
