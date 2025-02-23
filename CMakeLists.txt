cmake_minimum_required(VERSION 3.16)

# Keep lib/sdk at the end so we can override components from other libraries
list(APPEND EXTRA_COMPONENT_DIRS "lib/espp/components" "lib/esp-idf-lib/components" "lib/sdk")

set(EXCLUDE_COMPONENTS wrover-kit matouch-rotary-display motorgo-mini t-dongle-s3 esp32-timer-cam gfps_service codec qtpy neopixel)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("Smartknob-HA")