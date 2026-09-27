#include "pch_script.h"
#include "UISHoCDiaryWnd.h"

#include "UIXmlInit.h"
#include "UIHelper.h"
#include "UIInventoryUtilities.h"
#include "UINewsItemWnd.h"
#include "xrUICore/ListBox/UIListBox.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/TabControl/UITabControl.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "Actor.h"
#include "game_news.h"
#include "alife_registry_wrappers.h"

#define SHOC_DIARY_XML "events_new.xml"
#define SHOC_NEWS_XML "news.xml"

CUISHoCDiaryWnd::CUISHoCDiaryWnd() : CUIWindow("CUISHoCDiaryWnd") {}

bool CUISHoCDiaryWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_DIARY_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    // Left part: filter tabs and journal articles
    CUIWindow* left = this;
    if (CUIFrameWindow* frame = UIHelper::CreateFrameWindow(xml, "main_wnd:left_frame", this, false))
    {
        left = frame;
        CUIXmlInit::InitAutoStaticGroup(xml, "main_wnd:left_frame", 0, frame);
    }

    CUIWindow* left_header = UIHelper::CreateFrameLine(xml, "main_wnd:left_frame:left_frame_header", left, false);
    if (!left_header)
        left_header = left;

    constexpr pcstr filter_path = "main_wnd:left_frame:left_frame_header:filter_tab";
    if (xml.NavigateToNode(filter_path))
    {
        m_filter_tab = xr_new<CUITabControl>();
        m_filter_tab->SetAutoDelete(true);
        left_header->AttachChild(m_filter_tab);
        CUIXmlInit::InitTabControl(xml, filter_path, 0, m_filter_tab, false, true);
        m_filter_tab->SetMessageTarget(this);
    }

    constexpr pcstr anim_path = "main_wnd:left_frame:left_frame_header:anim_static";
    if (xml.NavigateToNode(anim_path))
    {
        auto* anim = xr_new<CUIAnimatedStatic>();
        anim->SetAutoDelete(true);
        left_header->AttachChild(anim);
        CUIXmlInit::InitAnimatedStatic(xml, anim_path, 0, anim);
    }

    CUIWindow* left_work_area = UIHelper::CreateNormalWindow(xml, "main_wnd:left_frame:work_area", left, false);
    m_src_list = UIHelper::CreateListBox(xml, "main_wnd:left_frame:work_area:src_list",
        left_work_area ? left_work_area : left, false);
    if (m_src_list)
        m_src_list->SetMessageTarget(this);

    // Right part: journal article or news
    CUIWindow* right = this;
    if (CUIFrameWindow* frame = UIHelper::CreateFrameWindow(xml, "main_wnd:right_frame", this, false))
    {
        right = frame;
        CUIXmlInit::InitAutoStaticGroup(xml, "main_wnd:right_frame", 0, frame);
    }
    UIHelper::CreateFrameLine(xml, "main_wnd:right_frame:right_frame_header", right, false);

    CUIWindow* right_work_area = UIHelper::CreateNormalWindow(xml, "main_wnd:right_frame:work_area", right, false);
    m_view = UIHelper::CreateScrollView(xml, "main_wnd:right_frame:work_area:scroll_view",
        right_work_area ? right_work_area : right, false);
    if (!m_view)
        return false;

    m_journal.Setup(m_src_list, m_view, nullptr, nullptr);

    return true;
}

void CUISHoCDiaryWnd::Show(bool status)
{
    if (status)
    {
        if (m_filter_tab && m_filter_tab->GetActiveIndex() < 0)
            m_filter_tab->SetActiveTabByIndex(0);

        const int index = m_filter_tab ? m_filter_tab->GetActiveIndex() : 0;
        ApplyFilter(index == int(EFilter::News) ? EFilter::News : EFilter::Journal);
    }
    else if (m_filter == EFilter::News)
        InventoryUtilities::SendInfoToActor("ui_pda_news_hide");

    inherited::Show(status);
}

void CUISHoCDiaryWnd::Update()
{
    if (m_filter == EFilter::News && NewsCount() != m_shown_news_count)
        FillNews();

    inherited::Update();
}

void CUISHoCDiaryWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (pWnd == m_filter_tab && msg == TAB_CHANGED)
    {
        const int index = m_filter_tab->GetActiveIndex();
        ApplyFilter(index == int(EFilter::News) ? EFilter::News : EFilter::Journal);
        return;
    }

    if (m_journal.OnListMessage(pWnd, msg, pData))
        return;

    inherited::SendMessage(pWnd, msg, pData);
}

void CUISHoCDiaryWnd::ApplyFilter(EFilter filter)
{
    if (m_filter == EFilter::News && filter != EFilter::News)
        InventoryUtilities::SendInfoToActor("ui_pda_news_hide");

    m_filter = filter;

    if (m_src_list)
        m_src_list->Show(filter == EFilter::Journal);

    if (filter == EFilter::Journal)
        m_journal.Rebuild();
    else
        FillNews();
}

size_t CUISHoCDiaryWnd::NewsCount()
{
    if (!Actor() || !Actor()->game_news_registry)
        return 0;
    return Actor()->game_news_registry->registry().objects().size();
}

void CUISHoCDiaryWnd::FillNews()
{
    m_view->Clear();
    m_shown_news_count = NewsCount();

    if (!m_shown_news_count)
        return;

    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_NEWS_XML, false))
        return;

    // Newest first
    GAME_NEWS_VECTOR& news = Actor()->game_news_registry->registry().objects();
    for (auto it = news.rbegin(); it != news.rend(); ++it)
    {
        auto* item = xr_new<CUINewsItemWnd>();
        item->Init(xml, it->m_type == GAME_NEWS_DATA::eTalk ? "talk_item" : "news_item");
        item->Setup(*it);
        m_view->AddWindow(item, true);
    }
}
