idf_component_register(SRC_DIRS "src"
                        INCLUDE_DIRS "include"
                        REQUIRES device_macro
                        PRIV_REQUIRES driver device_memory wifi_service device_common adc_reader i2c_adapter  periodic_task sound_generator clock_module st7567 DHT20
                    ) 