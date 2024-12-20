set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS src/ux.cpp)

idf_component_register(
        SRCS ${COMPONENT_SRCS}
        INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
        REQUIRES ring_lights light_sensor strain_sensor magnetic_encoder motor_driver display_driver
)