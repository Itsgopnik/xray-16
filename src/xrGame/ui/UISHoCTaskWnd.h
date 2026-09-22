#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"

class CUIStatic;
class CUIListBox;
class CUIScrollView;
class UIHint;

class CUISHoCTaskWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    CUIStatic* m_title{};
    CUIListBox* m_list{};
    CUIStatic* m_descr_title{};
    CUIScrollView* m_descr_view{};
    CUIStatic* m_empty_label{};

    void FillList();
    void ShowSelectedDescription();

public:
    UIHint* hint_wnd{};

    CUISHoCTaskWnd(UIHint* hint);
    ~CUISHoCTaskWnd() override;

    bool Init();
    void Show(bool status) override;
    void OnTaskSelected(CUIWindow* w, void* d);

    pcstr GetDebugType() override { return "CUISHoCTaskWnd"; }
};
