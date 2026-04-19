set(src_dirs
            EXIT
            WIFI
            HTTP
            UDP
            OLED)

set(include_dirs
            EXIT
            WIFI
            HTTP
            UDP
            OLED)

set(requires
            driver
            esp_wifi
            esp_netif   
            esp_event  
            nvs_flash   
            log
            esp_http_server
            lwip
            esp_driver_i2c
            esp_driver_gpio)

idf_component_register(SRC_DIRS "${src_dirs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)