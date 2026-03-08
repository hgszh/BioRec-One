# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mcu_ecg)

# run "idf.py merge" to generate a single all-in-one bin file
add_custom_target(merge
        COMMAND ${ESPTOOLPY} merge_bin -o ${chip_model}.bin @flash_args
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS gen_project_binary bootloader
        USES_TERMINAL
        VERBATIM
        )
        