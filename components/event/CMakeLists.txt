idf_component_register(SRCS "event.c"
                    INCLUDE_DIRS "include"

                    PRIV_REQUIRES 
                    REQUIRES      esp_event  esp_netif driver esp_wifi vfs
                    
                    )
