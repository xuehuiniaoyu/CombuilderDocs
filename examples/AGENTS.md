# Combuilder 应用工程 — Agent 约定

**本仓库是应用工程。** 按任务选 skill，用扩展命令模拟用户操作。

## 选哪个 skill

| 任务 | Skill |
|------|--------|
| 页面 / 组件 / Service / 预览编译 | `.cursor/skills/combuilder-app/`（先读 `agent-ops.md`） |
| 换板 / 引脚 / 显示按键音频 / 真机 SD | `.cursor/skills/combuilder-board/` |

## 应用侧命令（摘要）

`newActivity` / `newComponent` / `newService` → `compileAppProject` → 真机 `firmwareBuildAll` → `firmwareFlash`  

详情：`combuilder-app/agent-ops.md`、`widgets.md`、`howto.md`。

## 板级侧（摘要）

改 `components/esp32_ui_board/**`；选板用 `firmwareSelectBoard`。  
**禁止** LVGL 启动后清屏/直写面板。详情：`combuilder-board/SKILL.md`。

## 硬边界（两边共用）

- 勿改 `build/**`、`.combuilder-build/**`、生成框架头  
- 勿用 CMake Tools 替代 Combuidler 编译  
- 应用 skill 勿改 board；board skill 勿改 `src/ui`/`src/app` 业务
