idf_component_register(SRCS "factory_locator_main.c"
                       PRIV_REQUIRES esp_event esp_http_client esp_netif esp_timer esp_wifi nvs_flash spi_flash
                       INCLUDE_DIRS "")
