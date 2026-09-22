#include "pch_script.h"
#include "UIEncyclopediaWnd.h"

#include "Actor.h"
#include "UIHelper.h"
#include "UIXmlInit.h"
#include "alife_registry_wrappers.h"
#include "encyclopedia_article.h"
#include "xrUICore/ListBox/UIListBox.h"
#include "xrUICore/ListBox/UIListBoxItem.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/XML/UITextureMaster.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Windows/UITextFrameLineWnd.h"
#include "xrUICore/Windows/UIFrameWindow.h"

#define ENCYCLOPEDIA_XML "encyclopedia.xml"
#define ENCYCLOPEDIA_ITEM_XML "encyclopedia_item.xml"

namespace
{
    constexpr u32 kGroupTagMask = 0x80000000u;
    constexpr u32 kColorNormal = color_argb(255, 255, 255, 255);
    constexpr u32 kColorGroup = color_argb(255, 255, 255, 255);
    constexpr u32 kColorUnread = color_argb(255, 0, 255, 0);
    constexpr u32 kColorBody = color_argb(255, 255, 255, 255);
    constexpr pcstr kTreeIndent = "  ";

    void* EncodeArticleData(u32 articleIndex)
    {
        return reinterpret_cast<void*>(static_cast<intptr_t>(articleIndex) + 1);
    }

    void* EncodeGroupData(u32 groupIndex)
    {
        return reinterpret_cast<void*>(-static_cast<intptr_t>(groupIndex) - 1);
    }

    bool DecodeArticleData(void* data, u32& articleIndex)
    {
        const auto value = static_cast<intptr_t>(reinterpret_cast<intptr_t>(data));
        if (value <= 0)
            return false;

        articleIndex = static_cast<u32>(value - 1);
        return true;
    }

    bool DecodeGroupData(void* data, u32& groupIndex)
    {
        const auto value = static_cast<intptr_t>(reinterpret_cast<intptr_t>(data));
        if (value >= 0)
            return false;

        groupIndex = static_cast<u32>(-value - 1);
        return true;
    }

    u32 EncodeGroupTag(u32 groupIndex) { return kGroupTagMask | groupIndex; }
    bool IsGroupTag(u32 tag) { return (tag & kGroupTagMask) != 0; }
    u32 DecodeGroupTag(u32 tag) { return tag & ~kGroupTagMask; }
}

CUIEncyclopediaWnd::CUIEncyclopediaWnd() : CUIWindow("CUIEncyclopediaWnd") {}

bool CUIEncyclopediaWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, ENCYCLOPEDIA_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    // Draw the mask first so the actual pane content is rendered above it.
    m_mask_frame = UIHelper::CreateFrameWindow(xml, "mask_frame_window", this, false);
    m_info_frame = UIHelper::CreateFrameWindow(xml, "left_frame_window", this, false);
    m_index_frame = UIHelper::CreateFrameWindow(xml, "right_frame_window", this, false);

    CUIWindow* infoParent = m_info_frame ? static_cast<CUIWindow*>(m_info_frame) : static_cast<CUIWindow*>(this);
    CUIWindow* indexParent = m_index_frame ? static_cast<CUIWindow*>(m_index_frame) : static_cast<CUIWindow*>(this);

    m_info_header = UIHelper::CreateTextFrameLine(xml, "left_frame_line", infoParent, false);
    m_index_header = UIHelper::CreateTextFrameLine(xml, "right_frame_line", indexParent, false);

    if (xml.NavigateToNode("a_static"))
    {
        m_index_anim = xr_new<CUIAnimatedStatic>();
        m_index_anim->SetAutoDelete(true);
        indexParent->AttachChild(m_index_anim);
        CUIXmlInit::InitAnimatedStatic(xml, "a_static", 0, m_index_anim);
    }

    m_article_title = UIHelper::CreateStatic(xml, "article_header_static", infoParent, false);
    m_articles_list = UIHelper::CreateListBox(xml, "idx_list", indexParent, false);
    m_descr_view = UIHelper::CreateScrollView(xml, "info_list", infoParent, false);

    if (m_articles_list)
    {
        m_articles_list->SetMessageTarget(this);
    }

    CUIXmlInit::InitAutoStaticGroup(xml, "left_auto_static", 0, infoParent);
    CUIXmlInit::InitAutoStaticGroup(xml, "right_auto_static", 0, indexParent);

    return true;
}

void CUIEncyclopediaWnd::Show(bool status)
{
    if (status)
        RebuildList();
    inherited::Show(status);
}

void CUIEncyclopediaWnd::ClearDetail()
{
    if (m_article_title)
        m_article_title->SetText("");
    if (m_descr_view)
        m_descr_view->Clear();
}

void CUIEncyclopediaWnd::RebuildList()
{
    if (!m_articles_list)
        return;

    m_articles_list->Clear();
    m_article_ids.clear();
    m_article_groups.clear();
    m_article_unread.clear();
    m_group_ids.clear();
    ClearDetail();

    shared_str preferredExpandedGroup;
    bool havePreferredExpandedGroup = false;

    if (!m_selected_article_id.empty() && Actor() && Actor()->encyclopedia_registry)
    {
        ARTICLE_VECTOR& articles = Actor()->encyclopedia_registry->registry().objects();
        for (const ARTICLE_DATA& articleData : articles)
        {
            if (articleData.article_type == ARTICLE_DATA::eEncyclopediaArticle && articleData.article_id == m_selected_article_id)
            {
                CEncyclopediaArticle selectedArticle;
                selectedArticle.Load(articleData.article_id);
                preferredExpandedGroup = selectedArticle.data()->group;
                havePreferredExpandedGroup = true;
                break;
            }
        }
    }

    shared_str currentGroup;
    shared_str visibleGroup;
    u32 currentGroupIndex = u32(-1);
    u32 firstVisibleArticleIndex = u32(-1);
    u32 selectedVisibleArticleIndex = u32(-1);

    if (Actor() && Actor()->encyclopedia_registry)
    {
        ARTICLE_VECTOR& articles = Actor()->encyclopedia_registry->registry().objects();
        for (const ARTICLE_DATA& article_data : articles)
        {
            if (article_data.article_type != ARTICLE_DATA::eEncyclopediaArticle)
                continue;

            CEncyclopediaArticle article;
            article.Load(article_data.article_id);
            const xr_string groupName = StringTable().translate(article.data()->group).c_str();
            const xr_string articleName = StringTable().translate(article.data()->name).c_str();

            if (currentGroup != article.data()->group)
            {
                currentGroup = article.data()->group;
                currentGroupIndex = static_cast<u32>(m_group_ids.size());
                m_group_ids.push_back(currentGroup);

                const bool defaultExpanded =
                    havePreferredExpandedGroup ? currentGroup == preferredExpandedGroup : currentGroupIndex == 0;

                auto inserted = m_group_expanded.insert(std::make_pair(currentGroup, defaultExpanded));
                auto it = inserted.first;

                bool hasUnreadInGroup = false;
                for (const ARTICLE_DATA& groupArticleData : articles)
                {
                    if (groupArticleData.article_type != ARTICLE_DATA::eEncyclopediaArticle)
                        continue;

                    CEncyclopediaArticle groupArticle;
                    groupArticle.Load(groupArticleData.article_id);
                    if (groupArticle.data()->group == currentGroup && !groupArticleData.readed)
                    {
                        hasUnreadInGroup = true;
                        break;
                    }
                }

                AddGroupHeader((xr_string(it->second ? "-" : "+") + groupName).c_str(), currentGroupIndex, hasUnreadInGroup);
                if (visibleGroup.empty() && it->second)
                    visibleGroup = currentGroup;
            }

            const auto articleIndex = static_cast<u32>(m_article_ids.size());
            m_article_ids.push_back(article_data.article_id);
            m_article_groups.push_back(currentGroup);
            m_article_unread.push_back(!article_data.readed);

            if (!m_group_expanded[currentGroup] || currentGroupIndex == u32(-1))
                continue;

            CUIListBoxItem* item = m_articles_list->AddTextItem((xr_string(kTreeIndent) + articleName).c_str());
            item->SetTAG(articleIndex);
            item->SetData(EncodeArticleData(articleIndex));
            item->SetTextColor(article_data.readed ? kColorNormal : kColorUnread);
            item->SetTextureVisible(false);

            if (firstVisibleArticleIndex == u32(-1))
                firstVisibleArticleIndex = articleIndex;
            if (m_selected_article_id == article_data.article_id)
                selectedVisibleArticleIndex = articleIndex;
        }
    }

    if (m_index_header)
        m_index_header->SetText("Encyclopedia");

    const u32 articleToSelect = selectedVisibleArticleIndex != u32(-1) ? selectedVisibleArticleIndex : firstVisibleArticleIndex;
    if (articleToSelect != u32(-1))
    {
        m_articles_list->SetSelectedTAG(articleToSelect);
        // ShowArticle marks the article read and refreshes m_article_unread; only after
        // that can the group-header "has unread" colors be computed correctly.
        if (articleToSelect < m_article_ids.size())
            ShowArticle(m_article_ids[articleToSelect]);
    }
    UpdateSelectionColors();
}

void CUIEncyclopediaWnd::ShowArticle(const shared_str& article_id)
{
    if (!m_descr_view)
        return;

    m_descr_view->Clear();

    CEncyclopediaArticle article;
    article.Load(article_id);
    m_selected_article_id = article_id;
    MarkArticleRead(article_id);

    if (m_article_title)
        m_article_title->SetText(StringTable().translate(article.data()->name).c_str());
    if (m_info_header)
    {
        const xr_string breadcrumb =
            xr_string("# /") + StringTable().translate(article.data()->group).c_str() + "/" +
            StringTable().translate(article.data()->name).c_str();
        m_info_header->SetText(breadcrumb.c_str());
    }

    CUIXml itemXml;
    if (!itemXml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, ENCYCLOPEDIA_ITEM_XML, false))
    {
        auto* body = xr_new<CUIStatic>("ArticleBodyFallback");
        body->SetAutoDelete(true);
        body->SetTextComplexMode(true);
        body->SetWidth(m_descr_view->GetDesiredChildWidth());
        body->SetTextColor(kColorBody);
        body->SetText(StringTable().translate(article.data()->text.c_str()).c_str());
        body->AdjustHeightToText();
        m_descr_view->AddWindow(body, true);
        return;
    }

    auto* itemWnd = xr_new<CUIWindow>("EncyclopediaArticleItem");
    itemWnd->SetAutoDelete(true);
    CUIXmlInit::InitWindow(itemXml, "encyclopedia_wnd:objective_item", 0, itemWnd, false);

    CUIStatic* image = UIHelper::CreateStatic(itemXml, "encyclopedia_wnd:objective_item:image", itemWnd, false);
    if (image)
    {
        if (article.data()->image.GetShader() && article.data()->image.GetShader()->inited())
        {
            image->SetShader(article.data()->image.GetShader());
            image->SetTextureRect(article.data()->image.GetTextureRect());
            image->SetWndSize(article.data()->image.GetWndSize());
        }
        else
        {
            image->Show(false);
        }
    }

    CUIStatic* body = UIHelper::CreateStatic(itemXml, "encyclopedia_wnd:objective_item:text_cont", itemWnd, false);
    if (!body)
    {
        body = xr_new<CUIStatic>("ArticleBody");
        body->SetAutoDelete(true);
        itemWnd->AttachChild(body);
        body->SetWndPos({ 0.0f, 90.0f });
        body->SetWndSize({ m_descr_view->GetDesiredChildWidth(), 10.0f });
    }

    body->SetTextComplexMode(true);
    body->SetWidth(m_descr_view->GetDesiredChildWidth());
    body->SetTextColor(kColorBody);
    body->SetText(StringTable().translate(article.data()->text.c_str()).c_str());

    if (!image || !image->IsShown())
    {
        body->SetWndPos({ 0.0f, 8.0f });
    }
    else
    {
        body->SetWndPos({ 0.0f, 78.0f });
    }

    body->AdjustHeightToText();

    float contentBottom = body->GetWndPos().y + body->GetHeight();
    if (image && image->IsShown())
        contentBottom = _max(contentBottom, image->GetWndPos().y + image->GetHeight());

    itemWnd->SetWndSize({ m_descr_view->GetDesiredChildWidth(), contentBottom + 8.0f });
    m_descr_view->AddWindow(itemWnd, true);
}

void CUIEncyclopediaWnd::UpdateSelectionColors()
{
    if (!m_articles_list)
        return;

    // CUIListBox is a CUIScrollView — its visible items live under m_pad, not as
    // direct children. Iterate Items() to actually reach the listbox rows.
    for (CUIWindow* child : m_articles_list->Items())
    {
        CUIListBoxItem* item = smart_cast<CUIListBoxItem*>(child);
        if (!item)
            continue;

        u32 articleIndex = u32(-1);
        if (DecodeArticleData(item->GetData(), articleIndex))
        {
            const bool unread = articleIndex < m_article_unread.size() ? m_article_unread[articleIndex] : false;
            item->SetTextColor(unread ? kColorUnread : kColorNormal);
        }
        else
        {
            u32 groupIndex = u32(-1);
            bool hasUnread = false;
            if (DecodeGroupData(item->GetData(), groupIndex) && groupIndex < m_group_ids.size())
            {
                const shared_str& groupId = m_group_ids[groupIndex];
                for (size_t i = 0; i < m_article_groups.size() && i < m_article_unread.size(); ++i)
                {
                    if (m_article_groups[i] == groupId && m_article_unread[i])
                    {
                        hasUnread = true;
                        break;
                    }
                }
            }
            item->SetTextColor(hasUnread ? kColorUnread : kColorGroup);
        }
    }
}

void CUIEncyclopediaWnd::AddGroupHeader(pcstr text, u32 groupIndex, bool hasUnread)
{
    if (!m_articles_list)
        return;

    CUIListBoxItem* item = m_articles_list->AddTextItem(text);
    item->SetTAG(EncodeGroupTag(groupIndex));
    item->SetData(EncodeGroupData(groupIndex));
    item->SetTextColor(hasUnread ? kColorUnread : kColorGroup);
    item->SetTextureVisible(false);
}

void CUIEncyclopediaWnd::MarkArticleRead(const shared_str& article_id)
{
    if (!Actor() || !Actor()->encyclopedia_registry)
        return;

    ARTICLE_VECTOR& articles = Actor()->encyclopedia_registry->registry().objects();
    for (ARTICLE_DATA& articleData : articles)
    {
        if (articleData.article_type == ARTICLE_DATA::eEncyclopediaArticle && articleData.article_id == article_id)
        {
            articleData.readed = true;
            break;
        }
    }

    for (size_t i = 0; i < m_article_ids.size(); ++i)
    {
        if (m_article_ids[i] == article_id)
        {
            if (i < m_article_unread.size())
                m_article_unread[i] = false;
            break;
        }
    }
}

void CUIEncyclopediaWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (pWnd == m_articles_list && (msg == LIST_ITEM_CLICKED || msg == LIST_ITEM_SELECT))
    {
        const u32 tag = pData ? *static_cast<u32*>(pData) : u32(-1);
        if (tag == u32(-1))
            return;

        if (msg == LIST_ITEM_CLICKED && IsGroupTag(tag))
        {
            const u32 groupIndex = DecodeGroupTag(tag);
            if (groupIndex < m_group_ids.size())
            {
                const shared_str& groupId = m_group_ids[groupIndex];
                const bool shouldExpand = !m_group_expanded[groupId];
                for (const shared_str& existingGroupId : m_group_ids)
                    m_group_expanded[existingGroupId] = false;
                m_group_expanded[groupId] = shouldExpand;
                RebuildList();
            }
            return;
        }

        if (!IsGroupTag(tag) && tag < m_article_ids.size())
        {
            m_articles_list->SetSelectedTAG(tag);
            // Mark-read first so the group header's "has unread" recompute is fresh.
            ShowArticle(m_article_ids[tag]);
            UpdateSelectionColors();
        }
        return;
    }

    inherited::SendMessage(pWnd, msg, pData);
}
