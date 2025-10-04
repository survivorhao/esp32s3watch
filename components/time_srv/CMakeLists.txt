idf_component_register(SRCS "time_srv.c"
                    INCLUDE_DIRS "include"
                    
                    PRIV_REQUIRES
                    
                    REQUIRES      wifi_connect esp_timer
                    
                    )
