include($ENV{IDF_PATH}/tools/cmake/version.cmake) # $ENV{IDF_VERSION} was added after v4.3...

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "4.4")
    return()
endif()

# At the time of idf_component_register() we don't know which LVGL version is used, so we only register an INTERFACE component (with no sources)
# Later, when we know the LVGL version, we create another CMake library called 'lvgl_port_lib' and link it to the 'esp_lvgl_port' INTERFACE component
idf_component_register(
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "priv_include"
        REQUIRES "esp_lcd")

# Select folder by LVGL version
set(PORT_FOLDER "lvgl8")

# Add LVGL port extensions
set(PORT_PATH "src/${PORT_FOLDER}")

idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__button" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_button.c")
    list(APPEND  idf::espressif__button)
endif()
if("button" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_button.c")
    list(APPEND  idf::button)
endif()
if("espressif__esp_lcd_touch" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_touch.c")
    list(APPEND  idf::espressif__esp_lcd_touch)
endif()
if("esp_lcd_touch" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_touch.c")
    list(APPEND  idf::esp_lcd_touch)
endif()
if("espressif__knob" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_knob.c")
    list(APPEND  idf::espressif__knob)
endif()
if("knob" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_knob.c")
    list(APPEND  idf::knob)
endif()
if("espressif__usb_host_hid" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_usbhid.c" "images/${PORT_FOLDER}/img_cursor.c")
    list(APPEND  idf::espressif__usb_host_hid)
endif()
if("usb_host_hid" IN_LIST build_components)
    list(APPEND  "${PORT_PATH}/esp_lvgl_port_usbhid.c" "images/${PORT_FOLDER}/img_cursor.c")
    list(APPEND  idf::usb_host_hid)
endif()

# Here we create the real lvgl_port_lib
add_library(lvgl_port_lib STATIC
    ${PORT_PATH}/esp_lvgl_port.c
    ${PORT_PATH}/esp_lvgl_port_disp.c
    ${}
    )
target_include_directories(lvgl_port_lib PUBLIC "include")
target_include_directories(lvgl_port_lib PRIVATE "priv_include")
target_link_libraries(lvgl_port_lib PUBLIC
    idf::esp_lcd
    idf::lvgl
    )
target_link_libraries(lvgl_port_lib PRIVATE
    idf::esp_timer
    ${}
    )

# Finally, link the lvgl_port_lib its esp-idf interface library
target_link_libraries(${COMPONENT_LIB} INTERFACE lvgl_port_lib)
