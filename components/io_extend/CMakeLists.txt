idf_component_register(
                    SRCS "io_extend.c"
                    INCLUDE_DIRS "include"

                    PRIV_REQUIRES     
                    REQUIRES      freertos  bsp_driver 
                    
                    )
