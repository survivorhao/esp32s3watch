



idf_component_register(SRCS "test1.c"  
                    INCLUDE_DIRS "." 

                    PRIV_REQUIRES   sdcard  bsp_driver
                    io_extend    camera  
                    REQUIRES   my_ui  wifi_connect time_srv weather_srv
                    ble_srv
                    )
