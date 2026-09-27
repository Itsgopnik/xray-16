#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "GameTaskDefs.h"

class CUIXml;
class CUIStatic;
class CUI3tButton;
class CUIScrollView;
class CUITabControl;
class CUIMapWnd;
class CGameTask;
class UIHint;
class CUISHoCTaskWnd;

// One row of the SOC task list: a task (task_root_item) or one of its
// objectives (task_sub_item) from job_item.xml. Original: CUITaskItem.
class CUISHoCTaskItem : public CUIWindow
{
    using inherited = CUIWindow;

    CUISHoCTaskWnd* m_owner;
    CGameTask* m_task;
    TASK_OBJECTIVE_ID m_objective;

    // root item
    CUIStatic* m_image{};
    CUIStatic* m_caption{};
    CUIStatic* m_caption_time{};
    CUIStatic* m_rem_time{};
    CUI3tButton* m_switch_description_btn{};

    // sub item
    CUIStatic* m_state_image{};
    CUIStatic* m_description{};
    CUIStatic* m_active_objective_image{};
    CUI3tButton* m_show_descr_btn{};

    void InitRoot(CUIXml& xml);
    void InitSub(CUIXml& xml);
    bool IsActiveObjective() const;

public:
    CUISHoCTaskItem(CUISHoCTaskWnd* owner, CGameTask* task, TASK_OBJECTIVE_ID objective);

    void Init(CUIXml& xml);
    void MarkSelected(bool b);
    void Update() override;

    CGameTask* GameTask() const { return m_task; }
    TASK_OBJECTIVE_ID ObjectiveIdx() const { return m_objective; }

    bool OnMouseDown(int mouse_btn) override;
    bool OnDbClick() override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;

    pcstr GetDebugType() override { return "CUISHoCTaskItem"; }
};

// SOC "Tasks" PDA tab (original: CUIEventsWnd)
// Left: task list with active/accomplished/failed filter.
// Right: map focused on the selected objective, or the task's articles.
class CUISHoCTaskWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    enum class EFilter
    {
        Active = 0,
        Accomplished,
        Failed,
    };

    CUITabControl* m_filter_tab{};
    CUIScrollView* m_list{};
    CUIWindow* m_right_wnd{};
    CUIWindow* m_descr_wnd{};
    CUIScrollView* m_descr_view{};
    CUIMapWnd* m_map_wnd{};

    EFilter m_filter{ EFilter::Active };
    bool m_map_mode{ true };
    u64 m_tasks_signature{};

    bool Filter(CGameTask* task) const;
    u64 TasksSignature();
    void ReloadList();
    void ShowArticles(CGameTask* task, TASK_OBJECTIVE_ID objective);

public:
    UIHint* hint_wnd;

    CUISHoCTaskWnd(UIHint* hint);
    ~CUISHoCTaskWnd() override;

    bool Init();
    void Show(bool status) override;
    void Update() override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;

    bool IsMapMode() const { return m_map_mode; }
    void SetMapMode(bool map_mode);
    void ShowDescription(CGameTask* task, TASK_OBJECTIVE_ID objective);
    bool ItemHasDescription(const CUISHoCTaskItem* item) const;

    pcstr GetDebugType() override { return "CUISHoCTaskWnd"; }
};
