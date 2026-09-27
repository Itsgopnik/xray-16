#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "UIArticleBrowser.h"

class CUIListBox;
class CUIScrollView;
class CUITabControl;

// SOC "Diary" PDA tab (original: CUIDiaryWnd)
// Filter tabs switch between the journal (eJournalArticle articles) and the news log.
class CUISHoCDiaryWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    enum class EFilter
    {
        Journal = 0,
        News = 1,
    };

    CUITabControl* m_filter_tab{};
    CUIListBox* m_src_list{};
    CUIScrollView* m_view{};
    CUIArticleBrowser m_journal{ ARTICLE_DATA::eJournalArticle };

    EFilter m_filter{ EFilter::Journal };
    size_t m_shown_news_count{};

    void ApplyFilter(EFilter filter);
    void FillNews();
    size_t NewsCount();

public:
    CUISHoCDiaryWnd();

    bool Init();
    void Show(bool status) override;
    void Update() override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;

    pcstr GetDebugType() override { return "CUISHoCDiaryWnd"; }
};
