idf_component_register(SRCS "custom_event.c"
                    INCLUDE_DIRS "include"

                    PRIV_REQUIRES 
                    REQUIRES      esp_event  esp_netif driver esp_wifi vfs
                    esp_driver_gpio
                    )
