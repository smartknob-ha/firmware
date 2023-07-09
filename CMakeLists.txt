cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "lib/espp/components" "lib/sdk")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("Smartknob-HA")