#include "list.h"
#include "list_ui.h"
#include "alert.h"
#include "navigate.h"

void ListActivity::DemoAdapter::onItemClicked(int index) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Item %d", index);
    alert(buf);
}

void ListActivity::DemoAdapter::onItemDoubleClicked(int index) {
    navigateBack();
}

void ListActivity::buildUi(lv_obj_t * screen) {
    ui_ListActivity_build(screen);
}

void ListActivity::onCreate() {
    /* 和 Android 一样：setAdapter(适配器) 即可 */
    if (!setAdapter("list_659", &adapter_)) {
        std::printf("[ListActivity] setAdapter(list_659) 失败（控件未创建或不是 list）\n");
    }
}

void ListActivity::onStart() {
    /* JSON 热加载重建 list 后重新挂上 */
    if (!setAdapter("list_659", &adapter_)) {
        std::printf("[ListActivity] onStart setAdapter(list_659) 失败\n");
    }
}
