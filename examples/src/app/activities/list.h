#pragma once
#include "activity_base.h"
#include "ui_list.h"
#include <cstdio>

/** 演示：Android 风格 Adapter 绑定 1000 条 */
class ListActivity : public Activity {
public:
    void onCreate() override;
    void onStart() override;
    void buildUi(lv_obj_t * screen) override;

private:
    class DemoAdapter : public UiListAdapter {
    public:
        int getItemCount() const override { return 1000; }

        void onBindView(lv_obj_t * itemView, int index, bool selected) override {
            (void)selected;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Item %d", index);
            ui_list_item_set_text(itemView, "text_0", buf);
        }

        void onItemClicked(int index) override;
        void onItemDoubleClicked(int index) override;
    };

    DemoAdapter adapter_;
};
