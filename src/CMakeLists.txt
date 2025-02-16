# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources main.cpp)

idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "."
    PRIV_REQUIRES manager magnetic_encoder ring_lights light_sensor display_driver motor_driver)
