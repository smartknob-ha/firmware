set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS src/light_sensor.cpp)

idf_component_register(
        SRCS ${COMPONENT_SRCS}
        INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
        REQUIRES manager light_sensor veml7700
)