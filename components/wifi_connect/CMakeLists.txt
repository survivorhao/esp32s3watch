idf_component_register(SRCS "wifi_connect.c"
                    INCLUDE_DIRS "include"
                
                    PRIV_REQUIRES   
                    REQUIRES    nvs_flash  event
                    
                    
                    )
