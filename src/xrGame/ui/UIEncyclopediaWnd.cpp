#include "pch_script.h"
#include "UIEncyclopediaWnd.h"

#include "UIHelper.h"
#include "UIXmlInit.h"
#include "xrUICore/ListBox/UIListBox.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrUICore/Windows/UIFrameWindow.h"

#define ENCYCLOPEDIA_XML "encyclopedia.xml"

CUIEncyclopediaWnd::CUIEncyclopediaWnd() : CUIWindow("CUIEncyclopediaWnd") {}

bool CUIEncyclopediaWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, ENCYCLOPEDIA_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    // Draw the mask first so the actual pane content is rendered above it.
    UIHelper::CreateFrameWindow(xml, "mask_frame_window", this, false);
    CUIFrameWindow* info_frame = UIHelper::CreateFrameWindow(xml, "left_frame_window", this, false);
    CUIFrameWindow* index_frame = UIHelper::CreateFrameWindow(xml, "right_frame_window", this, false);

    CUIWindow* infoParent = info_frame ? static_cast<CUIWindow*>(info_frame) : static_cast<CUIWindow*>(this);
    CUIWindow* indexParent = index_frame ? static_cast<CUIWindow*>(index_frame) : static_cast<CUIWindow*>(this);

    CUIFrameLineWnd* info_header = UIHelper::CreateFrameLine(xml, "left_frame_line", infoParent, false);
    m_index_header = UIHelper::CreateFrameLine(xml, "right_frame_line", indexParent, false);

    if (xml.NavigateToNode("a_static"))
    {
        auto* index_anim = xr_new<CUIAnimatedStatic>();
        index_anim->SetAutoDelete(true);
        indexParent->AttachChild(index_anim);
        CUIXmlInit::InitAnimatedStatic(xml, "a_static", 0, index_anim);
    }

    CUIStatic* article_title = UIHelper::CreateStatic(xml, "article_header_static", infoParent, false);
    CUIListBox* articles_list = UIHelper::CreateListBox(xml, "idx_list", indexParent, false);
    CUIScrollView* descr_view = UIHelper::CreateScrollView(xml, "info_list", infoParent, false);

    if (articles_list)
        articles_list->SetMessageTarget(this);

    m_browser.Setup(articles_list, descr_view, article_title, info_header);

    UIHelper::CreateStatics(xml, "left_auto_static", infoParent);
    UIHelper::CreateStatics(xml, "right_auto_static", indexParent);

    return true;
}

void CUIEncyclopediaWnd::Show(bool status)
{
    if (status)
    {
        m_browser.Rebuild();
        if (m_index_header)
            m_index_header->GetTitleText(true)->SetText("Encyclopedia");
    }
    inherited::Show(status);
}

void CUIEncyclopediaWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (m_browser.OnListMessage(pWnd, msg, pData))
        return;

    inherited::SendMessage(pWnd, msg, pData);
}
