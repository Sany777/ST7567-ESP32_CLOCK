idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    EMBED_FILES embedded_files/index.html  embedded_files/script.js  embedded_files/style.css
                    PRIV_REQUIRES 
                    toolbox 
                    device_macro 
                    device_common 
                    esp_http_server 
                    clock_module 
                    wifi_service 
                    lwip 
                    freertos 
                    esp_netif
                    app_update
                )