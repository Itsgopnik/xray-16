#include "pch_script.h"
#include "UISHoCDiaryWnd.h"

#include "UIXmlInit.h"
#include "UISHoCPdaAux.h"
#include "UITreeViewItem.h"
#include "UIEncyclopediaArticleWnd.h"
#include "UINewsItemWnd.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/ListWnd/UIListWnd.h"
#include "xrUICore/TabControl/UITabControl.h"
#include "Actor.h"
#include "game_news.h"
#include "alife_registry_wrappers.h"

#define SHOC_DIARY_XML "events_new.xml"
#define SHOC_NEWS_XML "news.xml"
#define SHOC_ARTICLE_XML "encyclopedia_item.xml"

namespace
{
// The original PDA showed only the latest news
constexpr int kNewsToShow = 50;

template <typename T>
T* create(CUIWindow* parent, pcstr name)
{
    T* wnd = xr_new<T>(name);
    wnd->SetAutoDelete(true);
    parent->AttachChild(wnd);
    return wnd;
}
} // namespace

CUISHoCDiaryWnd::CUISHoCDiaryWnd() : CUIWindow("CUISHoCDiaryWnd") {}

CUISHoCDiaryWnd::~CUISHoCDiaryWnd()
{
    // Not auto-deleted: detached while the other filter is active
    delete_data(m_journal_list);
    delete_data(m_article_view);
    delete_data(m_news_view);
    delete_data(m_updated_sign);
    delete_data(m_old_sign);
}

bool CUISHoCDiaryWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_DIARY_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    auto* left_frame = create<CUIFrameWindow>(this, "left_frame");
    CUIXmlInit::InitFrameWindow(xml, "main_wnd:left_frame", 0, left_frame);
    CUIXmlInit::InitAutoStaticGroup(xml, "main_wnd:left_frame", 0, left_frame);

    auto* left_header = create<CUIFrameLineWnd>(left_frame, "left_frame_header");
    CUIXmlInit::InitFrameLine(xml, "main_wnd:left_frame:left_frame_header", 0, left_header);

    m_filter_tab = xr_new<CUITabControl>();
    m_filter_tab->SetAutoDelete(true);
    left_header->AttachChild(m_filter_tab);
    CUIXmlInit::InitTabControl(xml, "main_wnd:left_frame:left_frame_header:filter_tab", 0, m_filter_tab, true, true);
    Register(m_filter_tab);
    AddCallback(m_filter_tab, TAB_CHANGED, CUIWndCallback::void_function(this, &CUISHoCDiaryWnd::OnFilterChanged));

    if (xml.NavigateToNode("main_wnd:left_frame:left_frame_header:anim_static", 0))
    {
        auto* anim = xr_new<CUIAnimatedStatic>();
        anim->SetAutoDelete(true);
        left_header->AttachChild(anim);
        CUIXmlInit::InitAnimatedStatic(xml, "main_wnd:left_frame:left_frame_header:anim_static", 0, anim);
    }

    m_left_area = create<CUIWindow>(left_frame, "left_work_area");
    CUIXmlInit::InitWindow(xml, "main_wnd:left_frame:work_area", 0, m_left_area);

    m_journal_list = xr_new<CUIListWnd>();
    CUIXmlInit::InitListWnd(xml, "main_wnd:left_frame:work_area:src_list", 0, m_journal_list);
    Register(m_journal_list);
    AddCallback(m_journal_list, LIST_ITEM_CLICKED,
        CUIWndCallback::void_function(this, &CUISHoCDiaryWnd::OnJournalItemClicked));
    CUIXmlInit::InitFont(xml, "main_wnd:left_frame:work_area:src_list:tree_item_font", 0, m_tree_item_color, m_tree_item_font);
    CUIXmlInit::InitFont(xml, "main_wnd:left_frame:work_area:src_list:tree_root_font", 0, m_tree_root_color, m_tree_root_font);

    auto* right_frame = create<CUIFrameWindow>(this, "right_frame");
    CUIXmlInit::InitFrameWindow(xml, "main_wnd:right_frame", 0, right_frame);
    CUIXmlInit::InitAutoStaticGroup(xml, "main_wnd:right_frame", 0, right_frame);

    auto* right_header = create<CUIFrameLineWnd>(right_frame, "right_frame_header");
    CUIXmlInit::InitFrameLine(xml, "main_wnd:right_frame:right_frame_header", 0, right_header);

    m_right_area = create<CUIWindow>(right_frame, "right_work_area");
    CUIXmlInit::InitWindow(xml, "main_wnd:right_frame:work_area", 0, m_right_area);

    m_article_view = xr_new<CUIScrollView>();
    CUIXmlInit::InitScrollView(xml, "main_wnd:right_frame:work_area:scroll_view", 0, m_article_view);

    CUIXml news_xml;
    news_xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_NEWS_XML);
    m_news_view = xr_new<CUIScrollView>();
    CUIXmlInit::InitScrollView(news_xml, "list", 0, m_news_view);

    m_updated_sign = xr_new<CUIStatic>("updated_section_static");
    CUIXmlInit::InitStatic(xml, "updated_section_static", 0, m_updated_sign);
    m_old_sign = xr_new<CUIStatic>("old_section_static");
    CUIXmlInit::InitStatic(xml, "old_section_static", 0, m_old_sign);

    shoc_rearrange_tab_buttons(m_filter_tab, m_sign_places);
    return true;
}

void CUISHoCDiaryWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData) { CUIWndCallback::OnEvent(pWnd, msg, pData); }

void CUISHoCDiaryWnd::Show(bool status)
{
    inherited::Show(status);
    if (!status)
        return;
    if (m_filter_tab->GetActiveIndex() < 0)
        m_filter_tab->SetActiveTabByIndex(eJournal); // reports back through TAB_CHANGED
    else
        SetFilter(EDiaryFilter(m_filter_tab->GetActiveIndex()));
}

void CUISHoCDiaryWnd::OnFilterChanged(CUIWindow*, void*) { SetFilter(EDiaryFilter(m_filter_tab->GetActiveIndex())); }

void CUISHoCDiaryWnd::SetFilter(EDiaryFilter filter)
{
    // Unload the previous filter
    if (m_journal_list->GetParent())
        m_left_area->DetachChild(m_journal_list);
    if (m_article_view->GetParent())
        m_right_area->DetachChild(m_article_view);
    if (m_news_view->GetParent())
        m_right_area->DetachChild(m_news_view);
    m_article_view->Clear();

    m_filter = filter;
    switch (m_filter)
    {
    case eJournal:
        m_left_area->AttachChild(m_journal_list);
        m_right_area->AttachChild(m_article_view);
        UpdateJournal();
        g_pda_info_state &= ~pda_section::journal;
        break;
    case eNews:
        m_right_area->AttachChild(m_news_view);
        FillNews();
        g_pda_info_state &= ~pda_section::news;
        break;
    default: break;
    }
}

void CUISHoCDiaryWnd::UpdateJournal()
{
    CActor* actor = Actor();
    if (!actor || !actor->encyclopedia_registry)
        return;

    ARTICLE_VECTOR& registry = actor->encyclopedia_registry->registry().objects();
    if (registry.size() < m_known_articles)
    {
        // Registry shrank (game loaded): rebuild
        m_journal_list->RemoveAll();
        m_articles.clear();
        m_known_articles = 0;
    }

    for (auto it = registry.begin() + m_known_articles; it != registry.end(); ++it)
    {
        if (it->article_type != ARTICLE_DATA::eJournalArticle)
            continue;
        CEncyclopediaArticle& article = m_articles.emplace_back();
        article.Load(it->article_id);
        CreateTreeBranch(article.data()->group, article.data()->name, m_journal_list, int(m_articles.size() - 1),
            m_tree_root_font, m_tree_root_color, m_tree_item_font, m_tree_item_color, it->readed);
    }
    m_known_articles = registry.size();
}

void CUISHoCDiaryWnd::OnJournalItemClicked(CUIWindow*, void* data)
{
    auto* item = static_cast<CUITreeViewItem*>(data);
    m_article_view->Clear();
    if (!item || item->IsRoot() || item->GetValue() < 0 || item->GetValue() >= int(m_articles.size()))
        return;

    CEncyclopediaArticle& article = m_articles[item->GetValue()];
    auto* view = xr_new<CUIEncyclopediaArticleWnd>();
    view->Init(SHOC_ARTICLE_XML, "encyclopedia_wnd:objective_item");
    view->SetArticle(&article);
    m_article_view->AddWindow(view, true);

    // Remember that the entry was read (green until then)
    for (ARTICLE_DATA& data_item : Actor()->encyclopedia_registry->registry().objects())
    {
        if (data_item.article_type == ARTICLE_DATA::eJournalArticle && data_item.article_id == article.Id())
        {
            data_item.readed = true;
            break;
        }
    }
}

void CUISHoCDiaryWnd::FillNews()
{
    m_news_view->Clear();
    CActor* actor = Actor();
    if (!actor || !actor->game_news_registry)
        return;

    CUIXml news_xml;
    news_xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_NEWS_XML);

    GAME_NEWS_VECTOR& news = actor->game_news_registry->registry().objects();
    int shown = 0;
    for (auto it = news.rbegin(); it != news.rend() && shown < kNewsToShow; ++it, ++shown)
    {
        auto* item = xr_new<CUINewsItemWnd>();
        item->Init(news_xml, it->m_type == GAME_NEWS_DATA::eTalk ? "talk_item" : "news_item");
        item->Setup(*it);
        m_news_view->AddWindow(item, true);
    }
}

void CUISHoCDiaryWnd::Update()
{
    inherited::Update();
    if (m_journal_list->GetParent())
        ToggleTreeItemsIfPending(m_journal_list);
}

void CUISHoCDiaryWnd::Draw()
{
    inherited::Draw();
    m_updated_sign->Update();
    m_old_sign->Update();
    if (m_sign_places.size() > eNews)
    {
        shoc_draw_tab_sign(m_filter_tab, m_sign_places[eJournal], g_pda_info_state & pda_section::journal & ~pda_section::diary,
            m_updated_sign, m_old_sign);
        shoc_draw_tab_sign(m_filter_tab, m_sign_places[eNews], g_pda_info_state & pda_section::news & ~pda_section::diary,
            m_updated_sign, m_old_sign);
    }
}
