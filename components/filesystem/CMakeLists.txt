idf_component_register(
        SRCS "src/Filesystem.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES manager http_server semantic_versioning
                        PRIV_REQUIRES util esp_littlefs app_update esp_http_server mbedtls spi_flash)