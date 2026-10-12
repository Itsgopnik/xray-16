#pragma once
#include "xrUICore/Windows/UIWindow.h"

class CUIStatic;
class CEncyclopediaArticle;

// Article view (image + text) of the SoC PDA diary, laid out by encyclopedia_item.xml.
// Ported from the SoC/OGSR CUIEncyclopediaArticleWnd.
class CUIEncyclopediaArticleWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    CUIStatic* m_UIImage{};
    CUIStatic* m_UIText{};

    void AdjustLayout();

public:
    CUIEncyclopediaArticleWnd();

    void Init(pcstr xml_name, pcstr start_from);
    void SetArticle(CEncyclopediaArticle* article);

    pcstr GetDebugType() override { return "CUIEncyclopediaArticleWnd"; }
};
