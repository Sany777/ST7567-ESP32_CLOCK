idf_component_register(SRC_DIRS "src"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES toolbox  st7567 periodic_task clock_module wifi_service setting_server device_common forecast_http_client sound_generator adc_reader driver DHT20
                    ) 