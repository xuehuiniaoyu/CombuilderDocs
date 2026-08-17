# app_project：扩展「编译工程」产出；可用 -DESP32_UI_APP_PROJECT= 覆盖
#
# - set-target / menuconfig：无 app_project 也可配置通过
# - INCLUDE_DIRS 始终指向 app_project（避免「先 set-target、后编译工程」后找不到头文件）
# - 源文件用 CONFIGURE_DEPENDS 扫描：生成 app_project 后下次 build 会自动重配 cmake

if(NOT DEFINED ESP32_UI_APP_PROJECT)
    get_filename_component(_PROJ_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set(ESP32_UI_APP_PROJECT "${_PROJ_ROOT}/build/app_project")
endif()
get_filename_component(ESP32_UI_APP_PROJECT "${ESP32_UI_APP_PROJECT}" ABSOLUTE)

set(COMPONENT_REQUIRES
    esp32_ui_board
    nvs_flash
    joltwallet__littlefs
    lvgl
    esp_lvgl_port
    mbedtls
    esp_partition
    esp_driver_usb_serial_jtag
    esp_wifi
    esp_netif
    esp_event
    esp_http_server
    lwip
)

# IDF 早期依赖扫描：只登记 REQUIRES
if(CMAKE_BUILD_EARLY_EXPANSION)
    idf_component_register(
        SRCS "main.cpp" "debug_serial.cpp"
        INCLUDE_DIRS "."
        REQUIRES ${COMPONENT_REQUIRES}
    )
    return()
endif()

set(COMPONENT_SRCS "main.cpp" "debug_serial.cpp")
set(COMPONENT_ADD_INCLUDEDIRS
    "."
    "${ESP32_UI_APP_PROJECT}"
    "${ESP32_UI_APP_PROJECT}/activities"
    "${ESP32_UI_APP_PROJECT}/services"
    "${ESP32_UI_APP_PROJECT}/components"
    "${ESP32_UI_APP_PROJECT}/generated"
    "${ESP32_UI_APP_PROJECT}/generated/framework"
)

set(_ESP32_UI_APP_READY FALSE)
if(EXISTS "${ESP32_UI_APP_PROJECT}/generated/app_registry.cpp")
    set(_ESP32_UI_APP_READY TRUE)
endif()

# CONFIGURE_DEPENDS：app_project 从无到有时，ninja 会触发 cmake 重跑
file(GLOB APP_FW_SRCS CONFIGURE_DEPENDS
    "${ESP32_UI_APP_PROJECT}/generated/framework/*.c"
    "${ESP32_UI_APP_PROJECT}/generated/framework/*.cpp"
)
file(GLOB APP_REG_SRCS CONFIGURE_DEPENDS
    "${ESP32_UI_APP_PROJECT}/generated/app_registry.cpp"
)
file(GLOB APP_ACTIVITY_UI CONFIGURE_DEPENDS "${ESP32_UI_APP_PROJECT}/activities/*_ui.c")
file(GLOB APP_ACTIVITY_CPP CONFIGURE_DEPENDS "${ESP32_UI_APP_PROJECT}/activities/*.cpp")
file(GLOB APP_COMPONENT_UI CONFIGURE_DEPENDS "${ESP32_UI_APP_PROJECT}/components/*_ui.c")
file(GLOB APP_COMPONENT_CPP CONFIGURE_DEPENDS "${ESP32_UI_APP_PROJECT}/components/*.cpp")
file(GLOB APP_SERVICE_CPP CONFIGURE_DEPENDS "${ESP32_UI_APP_PROJECT}/services/*.cpp")
file(GLOB APP_USER_CPP CONFIGURE_DEPENDS "${ESP32_UI_APP_PROJECT}/*.cpp")
list(FILTER APP_USER_CPP EXCLUDE REGEX ".*/generated/.*")

if(APP_REG_SRCS)
    set(_ESP32_UI_APP_READY TRUE)
    list(APPEND COMPONENT_SRCS
        ${APP_FW_SRCS}
        ${APP_REG_SRCS}
        ${APP_ACTIVITY_UI}
        ${APP_ACTIVITY_CPP}
        ${APP_COMPONENT_UI}
        ${APP_COMPONENT_CPP}
        ${APP_SERVICE_CPP}
        ${APP_USER_CPP}
    )
    message(STATUS "ESP32_UI_APP_PROJECT=${ESP32_UI_APP_PROJECT}")
else()
    message(WARNING
        "未找到 build/app_project（缺 generated/app_registry.cpp）: ${ESP32_UI_APP_PROJECT}\n"
        "set-target / menuconfig 可继续；正式编译前请执行「Combuilder: 编译工程」。")
endif()

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
    REQUIRES ${COMPONENT_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP_PLATFORM=1)

if(NOT _ESP32_UI_APP_READY)
    add_custom_command(
        TARGET ${COMPONENT_LIB}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo
            "ERROR: missing build/app_project. Run Combuilder compile first, then idf.py build."
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()
