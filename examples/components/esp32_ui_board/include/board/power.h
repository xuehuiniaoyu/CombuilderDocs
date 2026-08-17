#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus

class BoardPower {
public:
    /** 上电保持（星智 POWER_LOCK=GPIO21 拉高） */
    esp_err_t init();
    /** 关机：拉低 lock + 可选 deep sleep */
    void shutdown();
    bool charging() const;
    bool lockEnabled() const { return lock_ok_; }

private:
    bool lock_ok_ = false;
};

#endif
