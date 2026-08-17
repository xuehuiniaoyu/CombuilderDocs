# examples — 示例应用（标准 ESP-IDF 工程）

本目录同时是 **UI 应用** 与 **ESP-IDF 工程根**（含 `CMakeLists.txt` / `main/` / `sdkconfig`）。

```
examples/
  CMakeLists.txt       # IDF 工程
  main/                # 入口 + 链接 UI 生成代码
  components/          # 板级 esp32_ui_board 等
  sdkconfig*
  partitions.csv
  src/                 # UI：app.json / JSON 布局 / Activity
    app.json
    ui/
    app/
  files/               # flash / sdcard 资源
  build/
    app_project/       # 扩展「编译工程」→ JSON→C（勿手改）
    idf/               # idf.py 构建目录（与 app_project 隔离）
    dist/              # LittleFS / assets 打包
```

## 职责

| 位置 | 角色 |
|------|------|
| 仓库根 `combuilder` | 框架：VS Code 扩展、模拟器、编译器 |
| `examples/` | **完整应用**：UI + 标准 IDF 固件 |

打开本目录即可使用 ESP-IDF 扩展状态栏（set-target / Build / Flash），无需再挂子文件夹。

## 开发流程

1. 改 `src/ui`、`src/app`
2. 扩展「编译工程」→ 更新 `build/app_project`
3. 固件：

```bash
cd examples
. $IDF_PATH/export.sh
idf.py -B build/idf set-target esp32s3
idf.py -B build/idf build
idf.py -B build/idf -p /dev/cu.usbmodem* flash monitor
```

或使用扩展底部「固件编译」面板 / ESP-IDF 状态栏（已配置 `idf.buildPath=build/idf`）。

板型：`idf.py menuconfig` → ESP32 UI Board，或扩展面板切换。  
引脚：`components/esp32_ui_board/boards/<板名>/`。
