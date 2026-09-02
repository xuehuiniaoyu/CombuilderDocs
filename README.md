# Demo45

## 首次安装 Python 依赖（生成 flash 镜像 / 图片处理会用到）

工作区根目录已带 `requirements.txt`，推荐用 Python 虚拟环境安装（避免污染全局 pip）：

```bash
# 1) 建虚拟环境（只需一次）
python -m venv .venv

# 2) 激活虚拟环境
# Windows (PowerShell):
.venv\Scripts\Activate.ps1
# Windows (cmd):
.venv\Scripts\activate.bat
# macOS / Linux:
source .venv/bin/activate

# 3) 装依赖（装完本项目 littlefs-python / Pillow 就齐了）
pip install -r requirements.txt
```

> `cmake / ninja / SDL2 / MinGW-gcc` 这类**系统级/二进制级依赖**，pip 装不了：
> 请打开**环境配置向导**（三种方式任选）：
> - 快捷键：`Ctrl+Alt+,`（Mac：`Cmd+Alt+,`）
> - 命令面板（`Ctrl+Shift+P`）→ 输入并执行「Combuilder 环境配置向导」
> 然后在各卡片底部点「一键安装/修复」按钮，或自行用系统包管理器（winget / brew / conda / apt）完成安装。
>
> 缺少工程结构时，可按 `Ctrl+Alt+.`（Mac：`Cmd+Alt+.`）或命令面板执行「生成 Combuilder 工程结构（新建/补齐）」。

## 常用快捷键

| 功能 | Windows / Linux | macOS |
|---|---|---|
| 打开环境配置向导 | Ctrl + Alt + , | Cmd + Alt + , |
| 生成 / 补齐 Combuilder 工程结构（仅未生成时可用） | Ctrl + Alt + . | Cmd + Alt + . |

## 开发架构总览

本工程基于 **Combuilder 低代码 UI 框架**，采用「**可视化 JSON 布局 + 原生 C/C++ 业务逻辑**」分离的模式开发，产物可一次编写、跨多嵌入式平台（ESP32 / STM32 / Linux / 模拟器）运行。

### 分层架构

```
+----------------------------------------------------------+
|                     业务层（你写的代码）                   |
|  +---------------------+    +-------------------------+  |
|  |   src/ui/*.json     |    |    src/app/*.cpp / *.h  |  |
|  |  可视化界面布局     |    |  业务逻辑、事件回调、    |  |
|  |  （拖拽/属性栏生成）|    |  数据绑定、外设驱动      |  |
|  +----------+----------+    +-----------+-------------+  |
+-------------+----------------------------+----------------+
              |                            |
              v                            v
+----------------------------------------------------------+
|              Combuilder 编译框架（build/app_project）      |
|  JSON->C 代码生成器  Activity 注册表  组件加载器          |
|  资源打包器（LittleFS / FAT16）  跨平台 HAL 抽象          |
+-------------------------------+--------------------------+
                                |
                                v
+----------------------------------------------------------+
|              平台适配层（platforms/<platform>）           |
|  +----------+  +--------------+  +--------------------+  |
|  | ESP32    |  | STM32(Cube)  |  | 桌面模拟器(SDL2)   |  |
|  | IDF      |  | HAL          |  | 本地调试预览       |  |
|  +----------+  +--------------+  +--------------------+  |
|  +----------------------------------------------------+  |
|  |   components/<platform>_ui_board . 板级硬件抽象     |  |
|  +----------------------------------------------------+  |
+----------------------------------------------------------+
```

### 核心约定

| 目录 / 文件 | 用途 | 可手改？ |
|---|---|---|
| `src/app.json` | 应用注册总入口：Activity 列表、启动页、自定义组件索引 | 是 |
| `src/ui/*.json` | 单个页面 / 组件的布局描述（由编辑器可视化生成） | 是，以编辑器为主 |
| `src/ui/components/*.json` | 自定义组件的布局（可被多页面复用） | 是，以编辑器为主 |
| `src/app/*.cpp` / `*.h` | 你的业务代码：Activity 类实现、事件回调、数据模型 | 是，主要写这里 |
| `files/flash/` | 打包进 LittleFS Flash 分区的资源（图片 / 字体 / 文本包） | 是 |
| `files/sdcard/` | 打包进 SD 卡的资源（大图片 / 长音频等） | 是 |
| `components/<platform>_ui_board/` | 当前平台的板级 HAL：屏幕驱动、按键、触摸、外设 | 是，板级移植时改 |
| `build/app_project/` | Combuilder 编译产出的 C 代码（ui_loader / app_registry / generated framework） | 否，由 JSON 重新生成，勿手改 |
| `build/dist/` | LittleFS / FAT16 二进制镜像，可直接烧录 | 否，编译产物 |
| `.combuilder/skills/` | 内置 Skill（应用 / 板级），辅助编辑器智能补全与自动化脚本 | 一般不改 |

### 编译与运行流程

```
 src/ui/*.json + src/app.json
          |
          v
   「Combuilder：编译工程」命令
   （或 Ctrl+Shift+P -> Compile）
          |
          +--->  JSON -> C：生成 build/app_project/
          |        ui_loader.h：从 JSON 还原 LVGL 树
          |        app_registry.h：Activity 路由表
          |        generated/framework/：存储、网络、时间等通用框架
          +--->  资源打包：files/flash -> LittleFS img；files/sdcard -> FAT16 img
          +--->  可选：启动桌面模拟器（LVGL SDL2 Preview）
                      预览模式：左侧 UI 编辑器
                      实时热加载：改 JSON 无需重启模拟器
                           |
                           v
                 platforms/<platform>/ 下原生构建
                           |
           ESP-IDF: idf.py build -> idf.py flash
           STM32  : CMake + CubeMX HAL -> ST-Link / DFU 烧录
           桌面    : cmake --build build -> ./<exe> 运行
```

### 常用开发模式

1. **UI 可视化设计**：双击 `src/ui/xxx.json` 打开 Combuilder 编辑器
   - 左侧：组件树；中间：实时预览（布局草图 + LVGL 模拟器帧，可切换）
   - 右侧：属性面板（尺寸/字体/颜色/事件绑定）
   - 选中组件 -> 点属性面板「...」-> 绑定 `onClicked` 等事件到 C++ 函数

2. **C++ 业务逻辑**：在 `src/app/` 下实现 Activity 类（继承自 ActivityBase）
   - `onCreate()`：初始化数据、注册事件回调
   - `onDestroy()`：释放资源
   - 通过 `findViewById<T>(id)` 获取控件引用并操作

3. **资源管理**：把 PNG / JPG / 字体（TTF/BDF）/ 文本 JSON 拖入 `files/flash/` 或 `files/sdcard/`
   - 编译时自动生成索引，C++ 端直接用路径引用即可

4. **板级移植**：新硬件 / 新屏幕 -> 修改 `components/<platform>_ui_board/` 下的 3 个接口
   - `board_disp_flush()`：屏幕刷帧回调
   - `board_indev_read()`：触摸 / 按键输入回调
   - `board_init()`：外设初始化

## 工程目录速查

```
Demo45.code-workspace                     # 推荐用此文件打开 VS Code
platforms/<platform>/                         # 平台工程（CMakeLists.txt + main + components）
  CMakeLists.txt / main / components/<platform>_ui_board
  build/                                      # 平台原生构建目录
requirements.txt                              # pip install -r requirements.txt（LittleFS 打包 / 图片处理）
src/
  app.json                                    # 应用入口（Activity 注册表 + 启动页）
  ui/                                         # JSON 布局（页面）
    components/                               # JSON 布局（可复用自定义组件）
  app/                                        # 手写 C/C++ 业务层
files/
  flash/                                      # LittleFS 分区资源
  sdcard/                                     # SD 卡资源
.combuilder/
  skills/combuilder-app/                      # 应用开发辅助 Skill
  skills/combuilder-board/                    # 板级移植辅助 Skill
.vscode/                                      # 编辑器配置（C/C++ 智能提示、格式化等）
build/
  app_project/                                # JSON->C 编译产出（勿手改）
  dist/                                       # LittleFS / FAT16 打包镜像
```

1. `src/ui` 用编辑器做可视化布局；`src/app` 写 C++ 业务
2. 执行「Combuilder：编译工程」-> 产物进 `build/app_project`
3. 进入对应 `platforms/<platform>/` 目录，用工具链原生构建并烧录
4. 切换板型时修改 `components/<platform>_ui_board` 下的硬件抽象
