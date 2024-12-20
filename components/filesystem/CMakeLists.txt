idf_component_register(
        SRCS "src/Filesystem.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES manager
                        PRIV_REQUIRES util esp_littlefs app_update)