idf_component_register(SRC_DIRS "src"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES additional_functions  periodic_task clock_module wifi_service setting_server device_common clock_http_client sound_generator  adc_reader driver st7567 DHT20
                    ) 