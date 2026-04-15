set(srcs
            "dynamo/dynamo.c")

set(include_dirs
            "dynamo")

set(requires
             "esp_driver_ledc" "esp_driver_pcnt" "esp_timer")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES "${requires}")
                    
component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)