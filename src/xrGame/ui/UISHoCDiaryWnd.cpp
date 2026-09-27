#include "pch_script.h"
#include "UISHoCDiaryWnd.h"

#include "UIXmlInit.h"
#include "UIInventoryUtilities.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "Actor.h"
#include "Level.h"
#include "game_news.h"
#include "alife_registry_wrappers.h"

#define SHOC_DIARY_XML "events_new.xml"

namespace
{
    constexpr u32 kColorTitle = color_argb(255, 231, 168, 12);
    constexpr u32 kColorCaption = color_argb(255, 231, 168, 12);
    constexpr u32 kColorText = color_argb(255, 225, 225, 250);
    constexpr u32 kColorMuted = color_argb(255, 181, 170, 148);

    constexpr float kViewW = 720.0f;
}

CUISHoCDiaryWnd::CUISHoCDiaryWnd() : CUIWindow("CUISHoCDiaryWnd") {}
CUISHoCDiaryWnd::~CUISHoCDiaryWnd() = default;

bool CUISHoCDiaryWnd::Init()
{
    SetWndPos({ 110.0f, 90.0f });
    SetWndSize({ 761.0f, 475.0f });

    CUIXml uiXml;
    uiXml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_DIARY_XML, false);

    m_title = xr_new<CUIStatic>("DiaryTitle");
    m_title->SetAutoDelete(true);
    AttachChild(m_title);
    m_title->SetWndPos({ 21.0f, 56.0f });
    m_title->SetWndSize({ 400.0f, 22.0f });
    m_title->TextItemControl()->SetTextColor(kColorTitle);
    m_title->TextItemControl()->SetTextST("st_ui_pda_diary_title");

    m_view = xr_new<CUIScrollView>();
    m_view->SetAutoDelete(true);
    AttachChild(m_view);
    m_view->SetWndPos({ 21.0f, 82.0f });
    m_view->SetWndSize({ kViewW, 381.0f });
    m_view->InitScrollView(); // creates m_pad and scrollbar

    m_empty_label = xr_new<CUIStatic>("DiaryEmpty");
    m_empty_label->SetAutoDelete(true);
    AttachChild(m_empty_label);
    m_empty_label->SetWndPos({ 21.0f, 200.0f });
    m_empty_label->SetWndSize({ kViewW, 22.0f });
    m_empty_label->TextItemControl()->SetTextColor(kColorMuted);
    m_empty_label->TextItemControl()->SetTextST("st_ui_pda_diary_empty");
    m_empty_label->Show(false);

    return true;
}

void CUISHoCDiaryWnd::Show(bool status)
{
    if (status)
        FillList();
    inherited::Show(status);
}

void CUISHoCDiaryWnd::FillList()
{
    if (!m_view)
        return;

    m_view->Clear();

    if (!Actor() || !Actor()->game_news_registry)
    {
        m_empty_label->Show(true);
        return;
    }

    GAME_NEWS_VECTOR& news = Actor()->game_news_registry->registry().objects();
    if (news.empty())
    {
        m_empty_label->Show(true);
        return;
    }
    m_empty_label->Show(false);

    // Newest entries first.
    for (auto it = news.rbegin(); it != news.rend(); ++it)
    {
        const GAME_NEWS_DATA& n = *it;

        shared_str date_str =
            InventoryUtilities::GetDateAsString(n.receive_time, InventoryUtilities::edpDateToDay);

        LPCSTR caption = n.news_caption.c_str();
        xr_string header = date_str.c_str();
        if (caption && xr_strlen(caption))
        {
            header += "  ";
            header += StringTable().translate(caption).c_str();
        }

        CUIStatic* cap = xr_new<CUIStatic>("DiaryCaption");
        cap->SetWndPos({ 0.0f, 0.0f });
        cap->SetWndSize({ kViewW - 20.0f, 18.0f });
        cap->SetTextColor(kColorCaption);
        cap->TextItemControl()->SetText(header.c_str());
        m_view->AddWindow(cap, true);

        LPCSTR body = n.news_text.c_str();
        if (body && xr_strlen(body))
        {
            CUIStatic* txt = xr_new<CUIStatic>("DiaryText");
            txt->SetWndPos({ 0.0f, 0.0f });
            txt->SetWndSize({ kViewW - 20.0f, 20.0f });
            txt->SetTextColor(kColorText);
            txt->SetTextComplexMode(true);
            txt->TextItemControl()->SetText(StringTable().translate(body).c_str());
            txt->AdjustHeightToText();
            m_view->AddWindow(txt, true);
        }

        // Blank spacer between entries.
        CUIStatic* spacer = xr_new<CUIStatic>("DiarySpacer");
        spacer->SetWndPos({ 0.0f, 0.0f });
        spacer->SetWndSize({ kViewW - 20.0f, 8.0f });
        m_view->AddWindow(spacer, true);
    }
}
