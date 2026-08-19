# Demo10

## AI / Agent

**请按照 [AGENTS.md](./AGENTS.md) 执行。**  
应用：`.cursor/skills/combuilder-app/`（`agent-ops.md` / `widgets.md`）。  
板级：`.cursor/skills/combuilder-board/`。  
用 Combuidler 命令编译/烧录；不要用 CMake Tools。

## 工程说明

标准 ESP-IDF 工程（与 Combuilder examples 同构）：

```
CMakeLists.txt / main / components/esp32_ui_board   # IDF 根 + board 驱动
src/
  app.json
  ui/                # JSON 布局
  app/               # 手写 C/C++
files/flash|sdcard   # 资源
AGENTS.md            # Agent 约定（必读，按此执行）
CLAUDE.md            # → AGENTS.md
.github/copilot-instructions.md  # VS Code Copilot
.cursor/skills/combuilder-app/     # 应用 Skill
.cursor/skills/combuilder-board/   # 板级 Skill
build/
  app_project/       # JSON→C（勿手改）
  idf/               # idf.py 构建目录
  dist/              # LittleFS / SD 打包
```

1. `src/ui` 写 JSON，`src/app` 写 C++（按 `AGENTS.md`）
2. 「编译工程」→ 产物进 `build/app_project`
3. 「固件编译」面板或 `idf.py -B build/idf build`（需已配置 IDF）
4. 板型在 `components/esp32_ui_board` / 固件面板中切换
