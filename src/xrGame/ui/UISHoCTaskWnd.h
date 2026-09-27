#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"
#include <xrCommon/xr_map.h>

class CUIStatic;
class CUIListBox;
class CUIScrollView;
class CUIAnimatedStatic;
class CUIFrameLineWnd;
class CUI3tButton;
class CUICheckButton;
class CGameTask;
class UIHint;
class CUIXml;

enum ETaskState : u32;

class CUISHoCTaskWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    // Layout following original SHoC ui/pda_events.xml
    CUIStatic* m_left_frame{};
    CUIFrameLineWnd* m_left_header{};
    CUIAnimatedStatic* m_anim_icon{};
    CUICheckButton* m_primary_only_check{};
    CUI3tButton* m_btn_filter_active{};
    CUI3tButton* m_btn_filter_accomplished{};
    CUI3tButton* m_btn_filter_failed{};
    CUIListBox* m_list{};
    CUIStatic* m_descr_header{};
    CUIStatic* m_descr_title_text{};   // frameline title of the right pane header
    CUIScrollView* m_descr_view{};
    CUIStatic* m_empty_label{};

    ETaskState m_filter_state{ static_cast<ETaskState>(1) }; // eTaskStateInProgress
    bool m_primary_only{ false };
    xr_map<CUIWindow*, ETaskState> m_filter_by_button;

    void FillList();
    void ShowSelectedDescription();
    void OnFilterClicked(CUIWindow* w, void* d);
    void OnPrimaryOnlyClicked(CUIWindow* w, void* d);

public:
    UIHint* hint_wnd{};

    CUISHoCTaskWnd(UIHint* hint);
    ~CUISHoCTaskWnd() override;

    bool Init();
    void Show(bool status) override;
    void OnTaskSelected(CUIWindow* w, void* d);

    pcstr GetDebugType() override { return "CUISHoCTaskWnd"; }
};
