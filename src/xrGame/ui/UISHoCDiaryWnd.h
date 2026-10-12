#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"
#include "encyclopedia_article.h"

class CUIFrameWindow;
class CUIFrameLineWnd;
class CUIAnimatedStatic;
class CUIStatic;
class CUITabControl;
class CUIScrollView;
class CUIListWnd;
class CGameFont;

// SoC PDA diary tab ("Logbuch" journal tree / news archive), built from the original
// events_new.xml (ported from the SoC/OGSR CUIDiaryWnd).
class CUISHoCDiaryWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    enum EDiaryFilter
    {
        eJournal = 0,
        eNews,
        eNone
    };
    EDiaryFilter m_filter{ eNone };

    CUIWindow* m_left_area{};
    CUIWindow* m_right_area{};
    CUITabControl* m_filter_tab{};
    CUIListWnd* m_journal_list{};
    CUIScrollView* m_article_view{};
    CUIScrollView* m_news_view{};

    CGameFont* m_tree_root_font{};
    u32 m_tree_root_color{};
    CGameFont* m_tree_item_font{};
    u32 m_tree_item_color{};

    xr_vector<Fvector2> m_sign_places;
    CUIStatic* m_updated_sign{};
    CUIStatic* m_old_sign{};

    // A deque, because CEncyclopediaArticle copies break the shared data's reference count
    xr_deque<CEncyclopediaArticle> m_articles;
    u32 m_known_articles{};

    void OnFilterChanged(CUIWindow*, void*);
    void OnJournalItemClicked(CUIWindow*, void*);
    void SetFilter(EDiaryFilter filter);
    void UpdateJournal();
    void FillNews();

public:
    CUISHoCDiaryWnd();
    ~CUISHoCDiaryWnd() override;

    bool Init();
    void Show(bool status) override;
    void Draw() override;
    void Update() override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;

    pcstr GetDebugType() override { return "CUISHoCDiaryWnd"; }
};
