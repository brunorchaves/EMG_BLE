# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "Modules/")
set(PROJECT_VER "1.0.0.0")
set(COMPONENT_EMBED_TXTFILES "certs/drive_google_com.pem")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(OXYGENIS.Firmware.BLE)
