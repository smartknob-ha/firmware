set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS src/LightSensor.cpp)

idf_component_register(
        SRCS ${COMPONENT_SRCS}
        INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
        REQUIRES manager light_sensor veml7700
        PRIV_REQUIRES util
)