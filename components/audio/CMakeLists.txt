idf_component_register(SRCS "audio_output.c" "audio_input.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES 
                    driver    es7210    es8311 
                    io_extend   bsp_driver   
                    REQUIRES      freertos

                    EMBED_FILES "canon.pcm"
                    )
