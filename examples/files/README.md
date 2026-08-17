# 资源目录

同一套相对路径，按介质放目录：

- `flash/<rel>` → LittleFS（`/flash/<rel>`），编译生成 `build/dist/flash_storage.bin`
- `sdcard/<rel>` → SD（`/sdcard/<rel>`），需额外拷贝

运行时统一：`storage_read_file("<rel>")`（不要在业务里区分介质）。
