idf_component_register(SRCS "camera.c"
                    INCLUDE_DIRS "include"
                    
                    PRIV_REQUIRES   io_extend bsp_driver  esp32-camera 
                    REQUIRES esp_image_effects event esp_lvgl_port ui


                    )
