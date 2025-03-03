#include "DepotRecognitionTask.h"

#include <future>

#include <meojson/json.hpp>

#include "Controller.h"
#include "DepotImageAnalyzer.h"
#include "GeneralConfiger.h"
#include "Logger.hpp"
#include "ProcessTask.h"
#include "TaskData.h"

bool asst::DepotRecognitionTask::_run()
{
    LogTraceFunction;

    bool ret = swipe_and_analyze();
    callback_analyze_result(true);

    return ret;
}

bool asst::DepotRecognitionTask::swipe_and_analyze()
{
    LogTraceFunction;
    m_all_items.clear();

    size_t pre_pos = 0ULL;
    while (true) {
        DepotImageAnalyzer analyzer(m_ctrler->get_image());

        auto future = std::async(std::launch::async, [&]() { swipe(); });

        // 因为滑动不是完整的一页，有可能上一次识别过的物品，这次仍然在页面中
        // 所以这个 begin pos 不能设置
        // analyzer.set_match_begin_pos(pre_pos);
        if (!analyzer.analyze()) {
            break;
        }
        size_t cur_pos = analyzer.get_match_begin_pos();
        if (cur_pos == pre_pos || cur_pos == DepotImageAnalyzer::NPos) {
            break;
        }
        pre_pos = cur_pos;

        auto cur_result = analyzer.get_result();
        m_all_items.merge(std::move(cur_result));

        future.wait();
        callback_analyze_result(false);
    }
    return m_all_items.empty();
}

void asst::DepotRecognitionTask::callback_analyze_result(bool done)
{
    LogTraceFunction;

    auto& templ = Configer.get_options().depot_export_template;
    json::value info = basic_info_with_what("DepotInfo");
    auto& details = info["details"];

    // https://penguin-stats.cn/planner
    if (auto arkplanner_template_opt = json::parse(templ.ark_planner)) {
        auto& arkplanner = details["arkplanner"];
        auto& arkplanner_obj = arkplanner["object"];
        arkplanner_obj = arkplanner_template_opt.value();
        auto& arkplanner_data_items = arkplanner_obj["items"];

        for (const auto& [item_id, item_info] : m_all_items) {
            arkplanner_data_items.array_emplace(json::object {
                { "id", item_id },
                { "have", item_info.quantity },
                { "name", item_info.item_name },
            });
        }
        arkplanner["data"] = arkplanner_obj.to_string();
    }

    // https://arkn.lolicon.app/#/material
    {
        auto& lolicon = details["lolicon"];
        auto& lolicon_obj = lolicon["object"];
        for (const auto& [item_id, item_info] : m_all_items) {
            lolicon_obj.object_emplace(item_id, item_info.quantity);
        }
        lolicon["data"] = lolicon_obj.to_string();
    }
    details["done"] = done;

    callback(AsstMsg::SubTaskExtraInfo, info);
}

void asst::DepotRecognitionTask::swipe()
{
    LogTraceFunction;
    static Rect right_rect = Task.get("DepotTaskSlowlySwipeRightRect")->specific_rect;
    static Rect left_rect = Task.get("DepotTaskSlowlySwipeLeftRect")->specific_rect;
    static int duration = Task.get("DepotTaskSlowlySwipeRightRect")->pre_delay;
    static int extra_delay = Task.get("DepotTaskSlowlySwipeRightRect")->rear_delay;

    m_ctrler->swipe(right_rect, left_rect, duration, true, extra_delay, true);
}
