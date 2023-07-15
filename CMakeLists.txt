cmake_minimum_required(VERSION 3.16)

# Keep lib/sdk at the end so we can override components from other libraries
list(APPEND EXTRA_COMPONENT_DIRS "lib/espp/components" "lib/esp-idf-lib/components" "lib/sdk")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("Smartknob-HA")