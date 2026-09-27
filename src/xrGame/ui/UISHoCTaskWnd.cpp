#include "pch_script.h"
#include "UISHoCTaskWnd.h"

#include "UIXmlInit.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/ListBox/UIListBox.h"
#include "xrUICore/ListBox/UIListBoxItem.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/Buttons/UI3tButton.h"
#include "xrUICore/Buttons/UICheckButton.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrUICore/Windows/UIWindow.h"
#include "Level.h"
#include "GametaskManager.h"
#include "GameTask.h"
#include "GameTaskDefs.h"

#include <xrCommon/xr_map.h>

#define SHOC_TASK_XML "pda_events.xml"

namespace
{
    // Fallback styling (approximates pda_events.xml when the XML is missing)
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
    const bool xmlLoaded = uiXml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_TASK_XML, false);
    const bool useXml = xmlLoaded && uiXml.NavigateToNode("main_wnd", 0);
    if (useXml)
    {
        // main_wnd in pda_events.xml is positioned against the whole PDA area;
        // CUIPdaWnd hosts sub-dialogs already offset by its own frame. Keep XML values,
        // they match the original absolute placement used by the old SoC engine.
        CUIXmlInit::InitWindow(uiXml, "main_wnd", 0, this);
    }
    else
        Msg("~ [SHOC-PDA] %s not found or missing main_wnd; using fallback layout", SHOC_TASK_XML);

    // ================================================================
    // LEFT PANE — task list (original pda_events.xml structure):
    //   main_wnd
    //     left_frame (0,0 340x475, texture ui_pda_frame_sub, a=90)
    //       auto_static x3 (decorative lines)
    //       left_frame_header (8,17 325x27, ui_string_02 + title "Tasks" + anim icon)
    //       list (0,48 334x421)
    //       primary_or_all (18,430 270x19)
    //       filter_tab (15,450 293x18) -> button x3
    // ================================================================
    m_left_frame = xr_new<CUIStatic>("SHoCTaskLeftFrame");
    m_left_frame->SetAutoDelete(true);
    AttachChild(m_left_frame);

    if (useXml && CUIXmlInit::InitStatic(uiXml, "main_wnd:left_frame", 0, m_left_frame, false))
    {
        const int alpha = uiXml.ReadAttribInt("main_wnd:left_frame", 0, "a", 255);
        if (alpha != 255)
            m_left_frame->SetTextureColor(color_argb(alpha, 255, 255, 255));
        CUIXmlInit::InitAutoStaticGroup(uiXml, "main_wnd:left_frame", 0, m_left_frame);
    }
    else
    {
        m_left_frame->SetWndPos({ 0.0f, 0.0f });
        m_left_frame->SetWndSize({ 340.0f, 475.0f });
    }

    // Header with title + animated icon — original uses frame line window with :title child
    m_left_header = xr_new<CUIFrameLineWnd>("SHoCTaskHeader");
    m_left_header->SetAutoDelete(true);
    m_left_frame->AttachChild(m_left_header);
    if (!(useXml && CUIXmlInit::InitFrameLine(uiXml, "main_wnd:left_frame:left_frame_header", 0, m_left_header, false)))
    {
        m_left_header->SetWndPos({ 8.0f, 17.0f });
        m_left_header->SetWndSize({ 325.0f, 27.0f });
        if (auto* titleText = m_left_header->GetTitleText(true))
        {
            titleText->TextItemControl()->SetTextColor(kColorTitle);
            titleText->TextItemControl()->SetTextST("st_ui_pda_tasks_title");
        }
    }

    if (useXml && CUIXmlInit::InitAnimatedStatic(uiXml, "main_wnd:left_frame:left_frame_header:anim_static", 0,
            m_anim_icon = xr_new<CUIAnimatedStatic>(), false))
    {
        m_anim_icon->SetAutoDelete(true);
        m_left_header->AttachChild(m_anim_icon);
    }

    // Task list
    m_list = xr_new<CUIListBox>();
    m_list->SetAutoDelete(true);
    m_left_frame->AttachChild(m_list);
    bool listOk = false;
    if (useXml)
    {
        const float x = uiXml.ReadAttribFlt("main_wnd:left_frame:list", 0, "x", 0.0f);
        const float y = uiXml.ReadAttribFlt("main_wnd:left_frame:list", 0, "y", 48.0f);
        const float w = uiXml.ReadAttribFlt("main_wnd:left_frame:list", 0, "width", 334.0f);
        const float h = uiXml.ReadAttribFlt("main_wnd:left_frame:list", 0, "height", 421.0f);
        m_list->SetWndPos(Fvector2().set(x, y));
        m_list->SetWndSize(Fvector2().set(w - 10.0f, h - 42.0f)); // right_ident/bottom_indent from XML
        listOk = true;
    }
    if (!listOk)
    {
        m_list->SetWndPos({ 0.0f, 48.0f });
        m_list->SetWndSize({ 324.0f, 379.0f });
    }
    m_list->SetTextColor(kColorRow);
    m_list->SetItemHeight(20.0f);
    m_list->SetMessageTarget(this);
    Register(m_list);
    AddCallback(m_list, LIST_ITEM_CLICKED, CUIWndCallback::void_function(this, &CUISHoCTaskWnd::OnTaskSelected));
    AddCallback(m_list, LIST_ITEM_SELECT, CUIWndCallback::void_function(this, &CUISHoCTaskWnd::OnTaskSelected));

    // "show only general tasks" checkbox (original primary_or_all)
    if (useXml && uiXml.NavigateToNode("main_wnd:left_frame:primary_or_all", 0))
    {
        m_primary_only_check = xr_new<CUICheckButton>();
        m_primary_only_check->SetAutoDelete(true);
        m_left_frame->AttachChild(m_primary_only_check);
        if (CUIXmlInit::InitCheck(uiXml, "main_wnd:left_frame:primary_or_all", 0, m_primary_only_check, false))
        {
            Register(m_primary_only_check);
            AddCallback(m_primary_only_check, BUTTON_CLICKED,
                CUIWndCallback::void_function(this, &CUISHoCTaskWnd::OnPrimaryOnlyClicked));
        }
        else
            m_primary_only_check->Show(false);
    }

    // Filter buttons: active / accomplished / failed (original filter_tab)
    if (useXml && uiXml.NavigateToNode("main_wnd:left_frame:filter_tab", 0))
    {
        struct BtnState { CUI3tButton** btn; ETaskState state; };
        const BtnState mapping[] =
        {
            { &m_btn_filter_active,       eTaskStateInProgress },
            { &m_btn_filter_accomplished, eTaskStateCompleted },
            { &m_btn_filter_failed,      eTaskStateFail },
        };

        // filter_tab is a positioning container: create it, then attach buttons with
        // their node-local coordinates into it.
        CUIWindow* filterContainer = xr_new<CUIWindow>("SHoCTaskFilterTab");
        filterContainer->SetAutoDelete(true);
        m_left_frame->AttachChild(filterContainer);
        filterContainer->SetWndPos(Fvector2().set(
            uiXml.ReadAttribFlt("main_wnd:left_frame:filter_tab", 0, "x", 15.0f),
            uiXml.ReadAttribFlt("main_wnd:left_frame:filter_tab", 0, "y", 450.0f)));
        filterContainer->SetWndSize(Fvector2().set(
            uiXml.ReadAttribFlt("main_wnd:left_frame:filter_tab", 0, "width", 293.0f),
            uiXml.ReadAttribFlt("main_wnd:left_frame:filter_tab", 0, "height", 18.0f)));

        for (u32 n = 0; n < 3; ++n)
        {
            CUI3tButton* btn = xr_new<CUI3tButton>();
            btn->SetAutoDelete(true);
            filterContainer->AttachChild(btn);
            if (CUIXmlInit::Init3tButton(uiXml, "main_wnd:left_frame:filter_tab:button", static_cast<int>(n), btn, false))
            {
                Register(btn);
                AddCallback(btn, BUTTON_CLICKED,
                    CUIWndCallback::void_function(this, &CUISHoCTaskWnd::OnFilterClicked));
                m_filter_by_button[btn] = mapping[n].state;
                *mapping[n].btn = btn;
            }
            else
                btn->Show(false);
        }
    }

    // ================================================================
    // RIGHT PANE — task description:
    //   right_frame (352,0 409x475)
    //     task_descr_view
    //       main_frame (0,0 409x475, ui_pda_frame_sub a=90, auto_statics)
    //         header_frame_line (8,17 395x21, ui_string_03, st_pda_task_detail_information)
    //         scroll_view (8,45 394x425)
    // ================================================================
    CUIWindow* rightFrame = xr_new<CUIWindow>("SHoCTaskRightFrame");
    rightFrame->SetAutoDelete(true);
    AttachChild(rightFrame);
    CUIStatic* rightMain = nullptr;
    if (useXml)
    {
        rightFrame->SetWndPos(Fvector2().set(
            uiXml.ReadAttribFlt("main_wnd:right_frame", 0, "x", 352.0f),
            uiXml.ReadAttribFlt("main_wnd:right_frame", 0, "y", 0.0f)));
        rightFrame->SetWndSize(Fvector2().set(
            uiXml.ReadAttribFlt("main_wnd:right_frame", 0, "width", 409.0f),
            uiXml.ReadAttribFlt("main_wnd:right_frame", 0, "height", 475.0f)));

        rightMain = xr_new<CUIStatic>("SHoCTaskRightMain");
        rightMain->SetAutoDelete(true);
        rightFrame->AttachChild(rightMain);
        if (CUIXmlInit::InitStatic(uiXml, "main_wnd:right_frame:task_descr_view:main_frame", 0, rightMain, false))
        {
            const int alpha = uiXml.ReadAttribInt("main_wnd:right_frame:task_descr_view:main_frame", 0, "a", 255);
            if (alpha != 255)
                rightMain->SetTextureColor(color_argb(alpha, 255, 255, 255));
            CUIXmlInit::InitAutoStaticGroup(uiXml, "main_wnd:right_frame:task_descr_view:main_frame", 0, rightMain);
        }
    }
    else
        rightFrame->SetWndRect(Frect().set(352.0f, 0.0f, 409.0f, 475.0f));

    // "detail information" header
    m_descr_header = xr_new<CUIStatic>("SHoCTaskDescrHeader");
    m_descr_header->SetAutoDelete(true);
    if (useXml && uiXml.NavigateToNode("main_wnd:right_frame:task_descr_view:main_frame:header_frame_line", 0))
    {
        auto* descrFrameLine = xr_new<CUIFrameLineWnd>("SHoCTaskDescrFrameLine");
        descrFrameLine->SetAutoDelete(true);
        (rightMain ? (CUIWindow*)rightMain : (CUIWindow*)rightFrame)->AttachChild(descrFrameLine);
        if (!CUIXmlInit::InitFrameLine(uiXml, "main_wnd:right_frame:task_descr_view:main_frame:header_frame_line", 0, descrFrameLine, false))
        {
            descrFrameLine->SetWndPos({ 8.0f, 17.0f });
            descrFrameLine->SetWndSize({ 395.0f, 21.0f });
            if (auto* titleText = descrFrameLine->GetTitleText(true))
            {
                titleText->TextItemControl()->SetTextColor(kColorTitle);
                titleText->TextItemControl()->SetTextST("st_pda_task_detail_information");
            }
        }
        // keep a pointer to the frameline's title text for updates on selection
        m_descr_title_text = descrFrameLine->GetTitleText();
    }
    else
    {
        (rightMain ? (CUIWindow*)rightMain : (CUIWindow*)rightFrame)->AttachChild(m_descr_header);
        m_descr_header->SetWndPos({ 8.0f, 17.0f });
        m_descr_header->SetWndSize({ 395.0f, 21.0f });
        m_descr_header->TextItemControl()->SetTextColor(kColorTitle);
        m_descr_header->TextItemControl()->SetTextST("st_pda_task_detail_information");
    }

    // Description scroll view
    m_descr_view = xr_new<CUIScrollView>();
    m_descr_view->SetAutoDelete(true);
    (rightMain ? (CUIWindow*)rightMain : (CUIWindow*)rightFrame)->AttachChild(m_descr_view);
    bool descrOk = false;
    if (useXml && uiXml.NavigateToNode("main_wnd:right_frame:task_descr_view:main_frame:scroll_view", 0))
    {
        if (CUIXmlInit::InitScrollView(uiXml, "main_wnd:right_frame:task_descr_view:main_frame:scroll_view", 0, m_descr_view, false))
            descrOk = true;
    }
    if (!descrOk)
    {
        m_descr_view->SetWndPos({ 8.0f, 45.0f });
        m_descr_view->SetWndSize({ 394.0f, 425.0f });
        m_descr_view->InitScrollView();
    }

    // Empty-list label
    m_empty_label = xr_new<CUIStatic>("TaskEmpty");
    m_empty_label->SetAutoDelete(true);
    m_left_frame->AttachChild(m_empty_label);
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

void CUISHoCTaskWnd::OnFilterClicked(CUIWindow* w, void*)
{
    const auto it = m_filter_by_button.find(w);
    if (it != m_filter_by_button.end())
    {
        m_filter_state = it->second;
        FillList();
    }
}

void CUISHoCTaskWnd::OnPrimaryOnlyClicked(CUIWindow*, void*)
{
    m_primary_only = m_primary_only_check ? m_primary_only_check->GetCheck() : false;
    FillList();
}

void CUISHoCTaskWnd::FillList()
{
    if (!m_list)
        return;

    m_list->Clear();
    m_descr_view->Clear();
    if (m_descr_title_text)
        m_descr_title_text->TextItemControl()->SetText("");
    else
        m_descr_header->TextItemControl()->SetText("");

    if (!g_pGameLevel)
    {
        m_empty_label->Show(true);
        return;
    }
    vGameTasks& tasks = Level().GameTaskManager().GetGameTasks();
    Msg("[SHOC-TASK] PDA TaskWnd: %d tasks in registry (all states), filter state=%d primary_only=%d",
        tasks.size(), static_cast<int>(m_filter_state), m_primary_only ? 1 : 0);
    u32 shown = 0;
    for (u32 i = 0; i < tasks.size(); ++i)
    {
        CGameTask* t = tasks[i].game_task;
        if (!t)
            continue;
        if (t->GetTaskState() != m_filter_state)
        {
            if (m_filter_state == eTaskStateInProgress) // log only for default filter to avoid spam
                Msg("[SHOC-TASK]   task[%d] id=[%s] skipped, state=%d", i, t->m_ID.c_str(), static_cast<int>(t->GetTaskState()));
            continue;
        }
        if (m_primary_only)
        {
            // "show only general tasks" — display only the currently active task
            CGameTask* active = Level().GameTaskManager().ActiveTask();
            if (active && t != active)
                continue;
        }

        TASK_OBJECTIVE_ID activeObj = t->ActiveObjectiveIdx();
        if (t->GetObjectivesCount() == 0 || activeObj >= t->GetObjectivesCount())
            activeObj = ROOT_TASK_OBJECTIVE;

        SGameTaskObjective& obj = t->Objective(activeObj);
        LPCSTR caption = obj.m_Title.c_str();
        if (!caption || !xr_strlen(caption))
            caption = t->m_Title.c_str();

        shared_str translated = StringTable().translate(caption);
        CUIListBoxItem* item = m_list->AddTextItem(
            translated.size() ? translated.c_str() : "<task>");
        item->SetTAG(i);
        item->SetTextColor(t->GetTaskState() == eTaskStateFail
            ? color_argb(255, 200, 80, 80)  // failed: reddish
            : kColorRow);
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
        if (m_descr_title_text)
            m_descr_title_text->TextItemControl()->SetText("");
        else
            m_descr_header->TextItemControl()->SetText("");
        return;
    }

    const u32 idx = sel->GetTAG();
    vGameTasks& tasks = Level().GameTaskManager().GetGameTasks();
    if (idx >= tasks.size())
        return;
    CGameTask* t = tasks[idx].game_task;
    if (!t)
        return;

    TASK_OBJECTIVE_ID activeObj = t->ActiveObjectiveIdx();
    if (t->GetObjectivesCount() == 0 || activeObj >= t->GetObjectivesCount())
        activeObj = ROOT_TASK_OBJECTIVE;

    SGameTaskObjective& obj = t->Objective(activeObj);

    LPCSTR title = obj.m_Title.c_str();
    if (!title || !xr_strlen(title))
        title = t->m_Title.c_str();

    // The frameline title doubles as the selected task caption
    if (m_descr_title_text)
        m_descr_title_text->TextItemControl()->SetText(StringTable().translate(title).c_str());
    else
        m_descr_header->TextItemControl()->SetText(StringTable().translate(title).c_str());

    LPCSTR descr = obj.m_Description.c_str();
    shared_str body = StringTable().translate(descr && xr_strlen(descr) ? descr : title);

    CUIStatic* txt = xr_new<CUIStatic>("TaskDescrBody");
    txt->SetWndPos({ 0.0f, 0.0f });
    txt->SetWndSize({ m_descr_view->GetWidth() - 20.0f, 20.0f });
    txt->SetTextColor(kColorRow);
    txt->SetTextComplexMode(true);
    txt->TextItemControl()->SetText(body.c_str());
    txt->AdjustHeightToText();
    m_descr_view->AddWindow(txt, true);
}
