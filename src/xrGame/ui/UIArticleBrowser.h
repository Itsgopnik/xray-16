#pragma once
#include "encyclopedia_article_defs.h"

class CUIWindow;
class CUIStatic;
class CUIListBox;
class CUIScrollView;
class CUIFrameLineWnd;

// Grouped, collapsible list of the actor's known articles of one type with a
// text view for the selected article. Shared by the SOC encyclopedia and the
// journal part of the SOC diary.
class CUIArticleBrowser
{
    ARTICLE_DATA::EArticleType m_type;

    CUIListBox* m_list{};
    CUIScrollView* m_descr_view{};
    CUIStatic* m_title{};
    CUIFrameLineWnd* m_header{};

    xr_vector<shared_str> m_article_ids;
    xr_vector<shared_str> m_article_groups;
    xr_vector<bool> m_article_unread;
    xr_vector<shared_str> m_group_ids;
    xr_map<shared_str, bool> m_group_expanded;
    shared_str m_selected_article_id;

    void ShowArticle(const shared_str& article_id);
    void UpdateSelectionColors();
    void AddGroupHeader(pcstr text, u32 groupIndex, bool hasUnread);
    void MarkArticleRead(const shared_str& article_id);

public:
    explicit CUIArticleBrowser(ARTICLE_DATA::EArticleType type) : m_type(type) {}

    // title and header are optional; header receives a "# /group/name" breadcrumb
    void Setup(CUIListBox* list, CUIScrollView* descr_view, CUIStatic* title, CUIFrameLineWnd* header);

    void Rebuild();
    void ClearDetail();

    // Returns true if the message was a list message handled by the browser
    bool OnListMessage(CUIWindow* pWnd, s16 msg, void* pData);

    // Appends image and text of an article to view, laid out by item_path in encyclopedia_item.xml
    static void AddArticleView(CUIScrollView* view, const shared_str& article_id, pcstr item_path);
};
