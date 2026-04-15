idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES BSP "esp_driver_uart" "esp_timer")
