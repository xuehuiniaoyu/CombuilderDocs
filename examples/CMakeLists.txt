# Combuilder — 示例应用（标准 ESP-IDF 工程根）
#
# UI 生成代码：build/app_project（扩展「编译工程」）
# IDF 构建目录：build/idf（与 app_project 隔离，见 .vscode/settings.json）

cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32s3 CACHE STRING "Target")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_ui_firmware)
