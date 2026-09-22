#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"

class CUIStatic;
class CUIScrollView;
class CUIListBox;
class CUIListBoxItem;
class CUIFrameWindow;
class CUIAnimatedStatic;
class CUITextFrameLineWnd;

class CUIEncyclopediaWnd final : public CUIWindow, public CUIWndCallback
{
    using inherited = CUIWindow;

    CUIFrameWindow* m_info_frame{};
    CUIFrameWindow* m_index_frame{};
    CUIFrameWindow* m_mask_frame{};
    CUITextFrameLineWnd* m_info_header{};
    CUITextFrameLineWnd* m_index_header{};
    CUIAnimatedStatic* m_index_anim{};
    CUIListBox* m_articles_list{};
    CUIStatic* m_article_title{};
    CUIScrollView* m_descr_view{};

    xr_vector<shared_str> m_article_ids;
    xr_vector<shared_str> m_article_groups;
    xr_vector<bool> m_article_unread;
    xr_vector<shared_str> m_group_ids;
    xr_map<shared_str, bool> m_group_expanded;
    shared_str m_selected_article_id;

public:
    CUIEncyclopediaWnd();
    ~CUIEncyclopediaWnd() override = default;

    bool Init();
    void Show(bool status) override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;

    pcstr GetDebugType() override { return "CUIEncyclopediaWnd"; }

private:
    void RebuildList();
    void ShowArticle(const shared_str& article_id);
    void ClearDetail();
    void UpdateSelectionColors();
    void AddGroupHeader(pcstr text, u32 groupIndex, bool expanded);
    void MarkArticleRead(const shared_str& article_id);
};
