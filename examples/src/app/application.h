#pragma once
#include "application_base.h"

/** 应用入口类：最先被拉起 */
class App : public Application {
public:
    void onCreate() override;
    void onTerminate() override;
};
