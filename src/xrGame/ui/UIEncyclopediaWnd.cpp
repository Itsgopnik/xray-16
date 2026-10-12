#include "pch_script.h"
#include "UIEncyclopediaWnd.h"

#include "UIXmlInit.h"
#include "UISHoCPdaAux.h"
#include "UITreeViewItem.h"
#include "UIEncyclopediaArticleWnd.h"
#include "Actor.h"
#include "alife_registry_wrappers.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/ListWnd/UIListWnd.h"
#include "xrEngine/StringTable/StringTable.h"

#define ENCYCLOPEDIA_XML "encyclopedia.xml"
#define ENCYCLOPEDIA_ITEM_XML "encyclopedia_item.xml"

namespace
{
constexpr pcstr kHeaderPrefix = "#";
}

CUIEncyclopediaWnd::CUIEncyclopediaWnd() : CUIWindow("CUIEncyclopediaWnd") {}

bool CUIEncyclopediaWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, ENCYCLOPEDIA_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    auto* index_frame = xr_new<CUIFrameWindow>("right_frame_window");
    index_frame->SetAutoDelete(true);
    AttachChild(index_frame);
    CUIXmlInit::InitFrameWindow(xml, "right_frame_window", 0, index_frame);

    CUIXmlInit::InitFont(xml, "tree_item_font", 0, m_tree_item_color, m_tree_item_font);
    CUIXmlInit::InitFont(xml, "tree_root_font", 0, m_tree_root_color, m_tree_root_font);

    auto* index_header = xr_new<CUIFrameLineWnd>("right_frame_line");
    index_header->SetAutoDelete(true);
    index_frame->AttachChild(index_header);
    CUIXmlInit::InitFrameLine(xml, "right_frame_line", 0, index_header);

    if (xml.NavigateToNode("a_static", 0))
    {
        auto* anim = xr_new<CUIAnimatedStatic>();
        anim->SetAutoDelete(true);
        index_header->AttachChild(anim);
        CUIXmlInit::InitAnimatedStatic(xml, "a_static", 0, anim);
    }

    auto* info_frame = xr_new<CUIFrameWindow>("left_frame_window");
    info_frame->SetAutoDelete(true);
    AttachChild(info_frame);
    CUIXmlInit::InitFrameWindow(xml, "left_frame_window", 0, info_frame);

    m_info_header = xr_new<CUIFrameLineWnd>("left_frame_line");
    m_info_header->SetAutoDelete(true);
    info_frame->AttachChild(m_info_header);
    CUIXmlInit::InitFrameLine(xml, "left_frame_line", 0, m_info_header);

    m_article_header = xr_new<CUIStatic>("article_header_static");
    m_article_header->SetAutoDelete(true);
    info_frame->AttachChild(m_article_header);
    CUIXmlInit::InitStatic(xml, "article_header_static", 0, m_article_header);

    m_index_list = xr_new<CUIListWnd>();
    m_index_list->SetAutoDelete(true);
    index_frame->AttachChild(m_index_list);
    CUIXmlInit::InitListWnd(xml, "idx_list", 0, m_index_list);
    m_index_list->SetMessageTarget(this);
    m_index_list->EnableScrollBar(true);

    m_info_list = xr_new<CUIScrollView>();
    m_info_list->SetAutoDelete(true);
    info_frame->AttachChild(m_info_list);
    CUIXmlInit::InitScrollView(xml, "info_list", 0, m_info_list);

    shoc_init_auto_statics(xml, "left_auto_static", info_frame);
    shoc_init_auto_statics(xml, "right_auto_static", index_frame);
    return true;
}

void CUIEncyclopediaWnd::Show(bool status)
{
    if (status)
    {
        UpdateArticles();
        g_pda_info_state &= ~pda_section::encyclopedia;
    }
    inherited::Show(status);
}

void CUIEncyclopediaWnd::Update()
{
    inherited::Update();
    // Outside of the list's loops (see CUITreeViewItem::OnClick)
    ToggleTreeItemsIfPending(m_index_list);
}

void CUIEncyclopediaWnd::UpdateArticles()
{
    CActor* actor = Actor();
    if (!actor || !actor->encyclopedia_registry)
        return;

    ARTICLE_VECTOR& registry = actor->encyclopedia_registry->registry().objects();
    if (registry.size() < m_known_articles)
    {
        // Registry shrank (game loaded): rebuild
        m_index_list->RemoveAll();
        m_articles.clear();
        m_known_articles = 0;
        m_info_list->Clear();
        m_article_header->SetText("");
        if (m_info_header->GetTitleText())
            m_info_header->GetTitleText()->SetText("");
    }

    for (auto it = registry.begin() + m_known_articles; it != registry.end(); ++it)
    {
        if (it->article_type != ARTICLE_DATA::eEncyclopediaArticle)
            continue;
        const bool known = std::any_of(m_articles.begin(), m_articles.end(),
            [&](CEncyclopediaArticle& a) { return a.Id() == it->article_id; });
        if (known)
            continue;
        CEncyclopediaArticle& article = m_articles.emplace_back();
        article.Load(it->article_id);
        CreateTreeBranch(article.data()->group, article.data()->name, m_index_list, int(m_articles.size() - 1),
            m_tree_root_font, m_tree_root_color, m_tree_item_font, m_tree_item_color, it->readed);
    }
    m_known_articles = registry.size();
}

void CUIEncyclopediaWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (pWnd == m_index_list && LIST_ITEM_CLICKED == msg)
        ShowArticle(static_cast<CUITreeViewItem*>(pData));
    inherited::SendMessage(pWnd, msg, pData);
}

void CUIEncyclopediaWnd::ShowArticle(CUITreeViewItem* item)
{
    m_info_list->ScrollToBegin();
    m_info_list->Clear();
    if (!item)
        return;

    // A group shows its name, an article the article
    const int index = item->vSubItems.empty() ? item->GetValue() : item->vSubItems.front()->GetValue();
    if (index < 0 || index >= int(m_articles.size()))
        return;
    CEncyclopediaArticle& article = m_articles[index];

    xr_string caption = kHeaderPrefix;
    caption += "/";
    caption += StringTable().translate(article.data()->group).c_str();
    if (item->vSubItems.empty())
    {
        caption += "/";
        caption += StringTable().translate(article.data()->name).c_str();
    }
    if (m_info_header->GetTitleText())
        m_info_header->GetTitleText()->SetText(caption.c_str());
    m_article_header->SetTextST(item->vSubItems.empty() ? article.data()->name.c_str() : article.data()->group.c_str());

    if (item->IsRoot())
        return;

    auto* view = xr_new<CUIEncyclopediaArticleWnd>();
    view->Init(ENCYCLOPEDIA_ITEM_XML, "encyclopedia_wnd:objective_item");
    view->SetArticle(&article);
    m_info_list->AddWindow(view, true);

    for (ARTICLE_DATA& data : Actor()->encyclopedia_registry->registry().objects())
    {
        if (data.article_type == ARTICLE_DATA::eEncyclopediaArticle && data.article_id == article.Id())
        {
            data.readed = true;
            break;
        }
    }
}
