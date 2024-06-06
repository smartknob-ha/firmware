set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS src/MotorDriver.cpp)

idf_component_register(
        SRCS ${COMPONENT_SRCS}
        INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
        REQUIRES manager filters bldc_haptics bldc_driver bldc_motor
        PRIV_REQUIRES util magnetic_encoder
)