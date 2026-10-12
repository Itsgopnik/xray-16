#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "encyclopedia_article.h"

class CUIStatic;
class CUIScrollView;
class CUIListWnd;
class CUIFrameWindow;
class CUIFrameLineWnd;
class CUITreeViewItem;
class CGameFont;

// SoC PDA encyclopedia ("Umwelt") tab: article tree on the left, article on the right,
// built from the original encyclopedia.xml (ported from the SoC/OGSR CUIEncyclopediaWnd).
class CUIEncyclopediaWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    CUIFrameLineWnd* m_info_header{};
    CUIStatic* m_article_header{};
    CUIListWnd* m_index_list{};
    CUIScrollView* m_info_list{};

    CGameFont* m_tree_root_font{};
    u32 m_tree_root_color{};
    CGameFont* m_tree_item_font{};
    u32 m_tree_item_color{};

    // A deque, because CEncyclopediaArticle copies break the shared data's reference count
    xr_deque<CEncyclopediaArticle> m_articles;
    u32 m_known_articles{};

    void UpdateArticles();
    void ShowArticle(CUITreeViewItem* item);

public:
    CUIEncyclopediaWnd();

    bool Init();
    void Show(bool status) override;
    void Update() override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;

    pcstr GetDebugType() override { return "CUIEncyclopediaWnd"; }
};
