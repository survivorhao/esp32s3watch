idf_component_register(SRCS "my_ui.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                    
                    REQUIRES  ui  custom_event  wifi_connect
                    )


