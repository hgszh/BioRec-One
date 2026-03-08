cmake_minimum_required(VERSION 3.12.4)

set(LVGL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE SOURCES ${LVGL_ROOT_DIR}/src/*.c)

if(CONFIG_LV_USE_DEMO_BENCHMARK)
  file(GLOB_RECURSE DEMO_BENCHMARK_SOURCES ${LVGL_ROOT_DIR}/demos/benchmark/*.c)
  list(APPEND DEMO_SOURCES ${DEMO_BENCHMARK_SOURCES})
endif()

idf_component_register(SRCS ${SOURCES} ${DEMO_SOURCES}
    INCLUDE_DIRS ${LVGL_ROOT_DIR} ${LVGL_ROOT_DIR}/src ${LVGL_ROOT_DIR}/../
                    ${LVGL_ROOT_DIR}/demos
    REQUIRES esp_timer)

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_CONF_INCLUDE_SIMPLE")

if(CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM)
  target_compile_definitions(${COMPONENT_LIB}
                             PUBLIC "-DLV_ATTRIBUTE_FAST_MEM=IRAM_ATTR")
endif()