set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS src/ux.cpp)

idf_component_register(
        SRCS ${COMPONENT_SRCS}
        INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
        REQUIRES manager strain_sensor ring_lights motor_driver light_sensor display_driver magnetic_encoder
)
