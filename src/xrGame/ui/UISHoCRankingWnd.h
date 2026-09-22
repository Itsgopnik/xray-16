#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"

class CUIStatic;
class CUIListBox;

class CUISHoCRankingWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    CUIStatic* m_title{};
    CUIStatic* m_header{};
    CUIListBox* m_list{};
    CUIStatic* m_empty_label{};

    void FillList();

public:
    CUISHoCRankingWnd();
    ~CUISHoCRankingWnd() override;

    bool Init();
    void Show(bool status) override;

    pcstr GetDebugType() override { return "CUISHoCRankingWnd"; }
};
