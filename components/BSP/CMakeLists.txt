set(src_dirs
            EXIT
            WIFI
            HTTP
            UDP)

set(include_dirs
            EXIT
            WIFI
            HTTP
            UDP)

set(requires
            esp_wifi
            esp_netif   
            esp_event  
            nvs_flash   
            driver
            log
            esp_http_server
            lwip)

idf_component_register(SRC_DIRS "${src_dirs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)