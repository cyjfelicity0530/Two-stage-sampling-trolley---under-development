set(src_dirs
            EXIT
            WIFI)

set(include_dirs
            EXIT
            WIFI)

set(requires
            esp_wifi
            esp_netif   
            esp_event  
            nvs_flash   
            driver)

idf_component_register(SRC_DIRS "${src_dirs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)