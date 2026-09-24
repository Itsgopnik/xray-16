#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"

class CUIStatic;
class CUIScrollView;

class CUISHoCDiaryWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    CUIStatic* m_title{};
    CUIScrollView* m_view{};
    CUIStatic* m_empty_label{};

    void FillList();

public:
    CUISHoCDiaryWnd();
    ~CUISHoCDiaryWnd() override;

    bool Init();
    void Show(bool status) override;

    pcstr GetDebugType() override { return "CUISHoCDiaryWnd"; }
};
