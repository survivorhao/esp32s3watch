idf_component_register(SRCS "weather_srv.c"
                    INCLUDE_DIRS "include"
                    
                    PRIV_REQUIRES
                    
                    REQUIRES      wifi_connect  esp_http_client json esp-tls

                    
                    )
