cmake_minimum_required(VERSION 3.16)

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello-world)