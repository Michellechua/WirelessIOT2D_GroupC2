idf_component_register(SRCS ./app_driver.c ./app_main.c
                       INCLUDE_DIRS "."
                       REQUIRES esp32-dht11 nvs_flash app_network app_insights app_reset
                       )
