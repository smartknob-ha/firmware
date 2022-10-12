cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "sdk")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(hello-world)