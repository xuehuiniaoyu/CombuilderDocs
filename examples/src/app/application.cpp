#include "application.h"
#include "io_service.h"
#include <cstdio>

void App::onCreate() {
    std::printf("[App] Clock demo — IoService 由 app.json 启动，供 MainActivity 读系统时间\n");
    auto * io = services().getAs<IoService>("IoService");
    if (!io) {
        std::printf("[App] 警告: IoService 未启动（检查 app.json services）\n");
    } else {
        std::printf("[App] IoService ready, workers=%d\n", io->workerCount());
    }
    /* 系统级 NTP：成功后 time() 全进程生效（请先在 app.json network 填 WiFi） */
    syncNtp();
}

void App::onTerminate() {
    std::printf("[App] App::onTerminate\n");
    stopService("IoService");
}
