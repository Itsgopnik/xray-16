#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "UIArticleBrowser.h"

class CUIFrameLineWnd;

class CUIEncyclopediaWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    CUIFrameLineWnd* m_index_header{};
    CUIArticleBrowser m_browser{ ARTICLE_DATA::eEncyclopediaArticle };

public:
    CUIEncyclopediaWnd();
    ~CUIEncyclopediaWnd() override = default;

    bool Init();
    void Show(bool status) override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;

    pcstr GetDebugType() override { return "CUIEncyclopediaWnd"; }
};
