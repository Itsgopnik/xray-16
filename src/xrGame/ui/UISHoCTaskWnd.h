#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"
#include "xrUICore/XML/xrUIXmlParser.h"

class CUIFrameWindow;
class CUIScrollView;
class CUITabControl;
class CUIMapWnd;
class CGameTask;
class UIHint;
class CUISHoCTaskDescrWnd;
class CUISHoCTaskItem;

// SoC PDA tasks tab: task list with filter tabs on the left, map or task articles on the right,
// built from the original pda_events.xml and job_item.xml (ported from the SoC/OGSR CUIEventsWnd).
class CUISHoCTaskWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    enum ETaskFilter
    {
        eActiveTask = 0,
        eAccomplishedTask,
        eFailedTask,
    };
    ETaskFilter m_filter{ eActiveTask };
    bool m_map_mode{ true };

    CUIWindow* m_right_wnd{};
    CUIMapWnd* m_map_wnd{};
    CUISHoCTaskDescrWnd* m_descr_wnd{};
    CUIScrollView* m_list{};
    CUITabControl* m_filter_tab{};

    bool Filter(CGameTask* task) const;
    void OnFilterChanged(CUIWindow*, void*);
    void ReloadList();

public:
    UIHint* hint_wnd{};
    CUIXml m_item_xml;

    CUISHoCTaskWnd(UIHint* hint);
    ~CUISHoCTaskWnd() override;

    bool Init();
    void Show(bool status) override;
    void Reset() override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;
    void DrawHint();

    void SetDescriptionMode(bool map);
    bool GetDescriptionMode() const { return m_map_mode; }
    void ShowDescription(CGameTask* task, u16 objective);
    bool ItemHasDescription(CUISHoCTaskItem* item) const;

    pcstr GetDebugType() override { return "CUISHoCTaskWnd"; }
};
