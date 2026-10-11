#include "pch_script.h"
#include "UISHoCTaskWnd.h"

#include "UIXmlInit.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/ListBox/UIListBox.h"
#include "xrUICore/ListBox/UIListBoxItem.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "Level.h"
#include "GametaskManager.h"
#include "GameTask.h"
#include "GameTaskDefs.h"

#define SHOC_TASK_XML "pda_events.xml"

namespace
{
    constexpr u32 kColorTitle = color_argb(255, 231, 168, 12);
    constexpr u32 kColorRow = color_argb(255, 225, 225, 250);
    constexpr u32 kColorMuted = color_argb(255, 181, 170, 148);
}

CUISHoCTaskWnd::CUISHoCTaskWnd(UIHint* hint) : CUIWindow("CUISHoCTaskWnd"), hint_wnd(hint) {}

CUISHoCTaskWnd::~CUISHoCTaskWnd() = default;

bool CUISHoCTaskWnd::Init()
{
    SetWndPos({ 110.0f, 90.0f });
    SetWndSize({ 761.0f, 475.0f });

    CUIXml uiXml;
    uiXml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_TASK_XML, false);

    // SoC panel chrome (programmatic; the 9-part atlas ids are registered by
    // the engine's SHoC compatibility texture table) — list frame, left:
    auto* list_frame = xr_new<CUIFrameWindow>("TaskListFrame");
    list_frame->SetAutoDelete(true);
    AttachChild(list_frame);
    list_frame->SetWndPos({ 14.0f, 46.0f });
    list_frame->SetWndSize({ 334.0f, 429.0f });
    list_frame->InitTexture("ui_pda2_mapframe");

    // Description frame, right:
    auto* descr_frame = xr_new<CUIFrameWindow>("TaskDescrFrame");
    descr_frame->SetAutoDelete(true);
    AttachChild(descr_frame);
    descr_frame->SetWndPos({ 353.0f, 46.0f });
    descr_frame->SetWndSize({ 395.0f, 429.0f });
    descr_frame->InitTexture("ui_pda2_mapframe");

    // Left pane: list of active tasks
    m_title = xr_new<CUIStatic>("TaskTitle");
    m_title->SetAutoDelete(true);
    AttachChild(m_title);
    m_title->SetWndPos({ 21.0f, 56.0f });
    m_title->SetWndSize({ 340.0f, 22.0f });
    m_title->TextItemControl()->SetTextColor(kColorTitle);
    m_title->TextItemControl()->SetTextST("st_ui_pda_tasks_title");

    m_list = xr_new<CUIListBox>();
    m_list->SetAutoDelete(true);
    AttachChild(m_list);
    m_list->SetWndPos({ 21.0f, 82.0f });
    m_list->SetWndSize({ 320.0f, 381.0f });
    m_list->SetItemHeight(18.0f);
    m_list->SetTextColor(kColorRow);
    m_list->SetMessageTarget(this);

    Register(m_list);
    AddCallback(m_list, LIST_ITEM_CLICKED,
        CUIWndCallback::void_function(this, &CUISHoCTaskWnd::OnTaskSelected));
    AddCallback(m_list, LIST_ITEM_SELECT,
        CUIWndCallback::void_function(this, &CUISHoCTaskWnd::OnTaskSelected));

    // Right pane: selected task description
    m_descr_title = xr_new<CUIStatic>("TaskDescrTitle");
    m_descr_title->SetAutoDelete(true);
    AttachChild(m_descr_title);
    m_descr_title->SetWndPos({ 360.0f, 56.0f });
    m_descr_title->SetWndSize({ 380.0f, 22.0f });
    m_descr_title->TextItemControl()->SetTextColor(kColorTitle);
    m_descr_title->TextItemControl()->SetText("");

    m_descr_view = xr_new<CUIScrollView>();
    m_descr_view->SetAutoDelete(true);
    AttachChild(m_descr_view);
    m_descr_view->SetWndPos({ 360.0f, 82.0f });
    m_descr_view->SetWndSize({ 380.0f, 381.0f });
    // Creates m_pad + the scrollbar (otherwise Clear()/AddWindow() dereferences
    // an uninitialized m_pad — crash fixed in the 24.09. session, kept for this port)
    m_descr_view->InitScrollView();

    m_empty_label = xr_new<CUIStatic>("TaskEmpty");
    m_empty_label->SetAutoDelete(true);
    AttachChild(m_empty_label);
    m_empty_label->SetWndPos({ 21.0f, 200.0f });
    m_empty_label->SetWndSize({ 320.0f, 22.0f });
    m_empty_label->TextItemControl()->SetTextColor(kColorMuted);
    m_empty_label->TextItemControl()->SetTextST("st_ui_pda_tasks_empty");
    m_empty_label->Show(false);

    return true;
}

void CUISHoCTaskWnd::Show(bool status)
{
    if (status)
        FillList();
    inherited::Show(status);
}

void CUISHoCTaskWnd::FillList()
{
    if (!m_list)
        return;

    m_list->Clear();
    if (m_descr_view)
        m_descr_view->Clear();
    if (m_descr_title)
        m_descr_title->TextItemControl()->SetText("");

    if (!g_pGameLevel)
    {
        m_empty_label->Show(true);
        return;
    }

    vGameTasks& tasks = Level().GameTaskManager().GetGameTasks();
    u32 shown = 0;
    for (u32 i = 0; i < tasks.size(); ++i)
    {
        CGameTask* t = tasks[i].game_task;
        if (!t)
            continue;
        if (t->GetTaskState() != eTaskStateInProgress)
            continue;

        SGameTaskObjective& obj = t->Objective(t->ActiveObjectiveIdx());
        LPCSTR caption = obj.m_Title.c_str();
        if (!caption || !xr_strlen(caption))
            caption = t->m_Title.c_str();

        shared_str translated = StringTable().translate(caption);
        CUIListBoxItem* item = m_list->AddTextItem(
            translated.size() ? translated.c_str() : "<task>");
        item->SetTAG(i);
        item->SetTextColor(kColorRow);
        ++shown;
    }

    if (shown == 0)
    {
        m_empty_label->Show(true);
        return;
    }
    m_empty_label->Show(false);

    m_list->SetSelectedIDX(0);
    ShowSelectedDescription();
}

void CUISHoCTaskWnd::OnTaskSelected(CUIWindow*, void*)
{
    ShowSelectedDescription();
}

void CUISHoCTaskWnd::ShowSelectedDescription()
{
    if (!m_descr_view || !m_list)
        return;

    m_descr_view->Clear();

    CUIListBoxItem* sel = m_list->GetSelectedItem();
    if (!sel || !g_pGameLevel)
    {
        if (m_descr_title)
            m_descr_title->TextItemControl()->SetText("");
        return;
    }

    const u32 idx = sel->GetTAG();
    vGameTasks& tasks = Level().GameTaskManager().GetGameTasks();
    if (idx >= tasks.size())
        return;
    CGameTask* t = tasks[idx].game_task;
    if (!t)
        return;

    SGameTaskObjective& obj = t->Objective(t->ActiveObjectiveIdx());

    LPCSTR title = obj.m_Title.c_str();
    if (!title || !xr_strlen(title))
        title = t->m_Title.c_str();
    if (m_descr_title)
        m_descr_title->TextItemControl()->SetText(StringTable().translate(title).c_str());

    LPCSTR descr = obj.m_Description.c_str();
    shared_str body = StringTable().translate(descr && xr_strlen(descr) ? descr : title);

    CUIStatic* txt = xr_new<CUIStatic>("TaskDescrBody");
    txt->SetWndPos({ 0.0f, 0.0f });
    txt->SetWndSize({ 360.0f, 20.0f });
    txt->SetTextColor(kColorRow);
    txt->SetTextComplexMode(true);
    txt->TextItemControl()->SetText(body.c_str());
    txt->AdjustHeightToText();
    m_descr_view->AddWindow(txt, true);
}
