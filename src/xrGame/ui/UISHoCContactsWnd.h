#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"

class CUIStatic;
class CUIListBox;

class CUISHoCContactsWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    CUIStatic* m_title{};
    CUIListBox* m_list{};
    CUIStatic* m_detail_name{};
    CUIStatic* m_detail_faction{};
    CUIStatic* m_detail_rank{};
    CUIStatic* m_detail_attitude{};
    CUIStatic* m_empty_label{};

    void FillList();
    void ShowSelectedDetail();

public:
    CUISHoCContactsWnd();
    ~CUISHoCContactsWnd() override;

    bool Init();
    void Show(bool status) override;
    void OnContactSelected(CUIWindow* w, void* d);

    pcstr GetDebugType() override { return "CUISHoCContactsWnd"; }
};
