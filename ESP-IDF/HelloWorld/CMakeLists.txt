# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# how to make Serial.print() work!
# https://esp32.com/viewtopic.php?t=30926
add_definitions(
    -DARDUINO_USB_MODE
    -DARDUINO_USB_CDC_ON_BOOT
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(HelloWorld)

