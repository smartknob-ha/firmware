set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS source/DisplayDriver.cpp)

idf_component_register(
        SRCS ${COMPONENT_SRCS}
        INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
        PRIV_REQUIRES spi_flash esp_lcd display display_drivers driver task format
        REQUIRES manager
)