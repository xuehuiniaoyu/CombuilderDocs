idf_component_register(
    SRCS
        "src/board.cpp"
        "src/display_st7789.cpp"
        "src/input_buttons.cpp"
        "src/audio_i2s_simplex.cpp"
        "src/power.cpp"
        "src/ui_platform.cpp"
    INCLUDE_DIRS
        "include"
        "boards"
    REQUIRES
        driver
        esp_lcd
        esp_timer
        esp_driver_i2s
        esp_lvgl_port
        lvgl
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
