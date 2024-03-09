set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS src/RightLights.cpp
        src/Effects.cpp)

idf_component_register(
        SRCS ${COMPONENT_SRCS}
        INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
        REQUIRES led_strip manager ring_lights
)