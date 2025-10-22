idf_component_register(SRCS "ble_srv.c" "esp_hid_gap.c"
                    INCLUDE_DIRS "include"
                    
                    PRIV_REQUIRES
                    
                    REQUIRES  custom_event bt nvs_flash esp_hid driver
                    
                    )
