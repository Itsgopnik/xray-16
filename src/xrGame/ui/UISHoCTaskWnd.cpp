#include "pch_script.h"
#include "UISHoCTaskWnd.h"

#include "UIXmlInit.h"
#include "UIHelper.h"
#include "UIInventoryUtilities.h"
#include "UIMapWnd.h"
#include "UIArticleBrowser.h"
#include "xrUICore/Buttons/UI3tButton.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/TabControl/UITabControl.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "Actor.h"
#include "Level.h"
#include "GametaskManager.h"
#include "GameTask.h"
#include "map_location.h"
#include "encyclopedia_article.h"
#include "alife_registry_wrappers.h"

#define SHOC_TASK_XML "pda_events.xml"
#define SHOC_TASK_ITEM_XML "job_item.xml"

namespace
{
constexpr pcstr TEX_SHOW_TEXT = "ui_icons_newPDA_showtext";
constexpr pcstr TEX_SHOW_MAP = "ui_icons_newPDA_showmap";
constexpr pcstr DESCR_ITEM_PATH = "events_wnd:objective_item";

bool IsCursorOver(const CUIWindow* w, Fvector2 cursor)
{
    return w && w->IsShown() && w->GetWndRect().in(cursor);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// CUISHoCTaskItem

CUISHoCTaskItem::CUISHoCTaskItem(CUISHoCTaskWnd* owner, CGameTask* task, TASK_OBJECTIVE_ID objective)
    : CUIWindow("CUISHoCTaskItem"), m_owner(owner), m_task(task), m_objective(objective) {}

void CUISHoCTaskItem::Init(CUIXml& xml)
{
    if (m_objective == ROOT_TASK_OBJECTIVE)
        InitRoot(xml);
    else
        InitSub(xml);
}

void CUISHoCTaskItem::InitRoot(CUIXml& xml)
{
    CUIXmlInit::InitWindow(xml, "task_root_item", 0, this);

    m_image = UIHelper::CreateStatic(xml, "task_root_item:image", this, false);
    m_caption = UIHelper::CreateStatic(xml, "task_root_item:caption", this, false);
    m_caption_time = UIHelper::CreateStatic(xml, "task_root_item:caption_time", this, false);
    m_rem_time = UIHelper::CreateStatic(xml, "task_root_item:rem_time", this, false);
    m_switch_description_btn = UIHelper::Create3tButton(xml, "task_root_item:switch_description_btn", this, false);

    float height = 0.0f;

    if (m_image)
    {
        if (m_task->m_icon_texture_name.size() && m_image->InitTexture(m_task->m_icon_texture_name.c_str(), false))
        {
            // m_icon_rect stores x, y, width, height
            const Frect& r = m_task->m_icon_rect;
            m_image->SetTextureRect({ r.x1, r.y1, r.x1 + r.x2, r.y1 + r.y2 });
            m_image->SetStretchTexture(true);
        }
        height = m_image->GetWndPos().y + m_image->GetHeight();
    }

    float text_bottom = 0.0f;
    if (m_caption)
    {
        m_caption->SetTextST(m_task->m_Title.c_str());
        m_caption->AdjustHeightToText();
        text_bottom = m_caption->GetWndPos().y + m_caption->GetHeight();
    }

    if (m_caption_time)
    {
        xr_string txt = InventoryUtilities::GetDateAsString(m_task->m_ReceiveTime, InventoryUtilities::edpDateToDay).c_str();
        txt += " ";
        txt += InventoryUtilities::GetTimeAsString(m_task->m_ReceiveTime, InventoryUtilities::etpTimeToMinutes).c_str();
        m_caption_time->SetText(txt.c_str());
        m_caption_time->SetWndPos({ m_caption_time->GetWndPos().x, text_bottom + 3.0f });
        text_bottom = m_caption_time->GetWndPos().y + m_caption_time->GetHeight();
    }

    if (m_rem_time)
    {
        const bool has_time_limit = m_task->GetTaskState() == eTaskStateInProgress &&
            m_task->m_TimeToComplete > m_task->m_ReceiveTime;
        m_rem_time->Show(has_time_limit);
        if (has_time_limit)
        {
            m_rem_time->SetWndPos({ m_rem_time->GetWndPos().x, text_bottom });
            text_bottom = m_rem_time->GetWndPos().y + m_rem_time->GetHeight();
        }
    }

    height = _max(height, text_bottom);
    if (m_switch_description_btn)
        height = _max(height, m_switch_description_btn->GetWndPos().y + m_switch_description_btn->GetHeight());

    SetHeight(height);
}

void CUISHoCTaskItem::InitSub(CUIXml& xml)
{
    CUIXmlInit::InitWindow(xml, "task_sub_item", 0, this);

    m_state_image = UIHelper::CreateStatic(xml, "task_sub_item:state_image", this, false);
    m_description = UIHelper::CreateStatic(xml, "task_sub_item:description", this, false);
    m_active_objective_image = UIHelper::CreateStatic(xml, "task_sub_item:active_objecttive_image", this, false);
    m_show_descr_btn = UIHelper::Create3tButton(xml, "task_sub_item:show_descr_btn", this, false);

    const SGameTaskObjective& objective = m_task->Objective(m_objective);

    float height = GetHeight();
    if (m_description)
    {
        m_description->SetTextST(objective.m_Description.c_str());
        m_description->AdjustHeightToText();
        height = m_description->GetWndPos().y + m_description->GetHeight();
    }
    if (m_active_objective_image)
        height = _max(height, m_active_objective_image->GetWndPos().y + m_active_objective_image->GetHeight());
    SetHeight(height);

    pcstr state_texture = nullptr;
    pcstr color_path = nullptr;
    switch (objective.GetTaskState())
    {
    case eTaskStateFail:
        state_texture = "ui_icons_PDA_subtask_failed";
        color_path = "task_sub_item:description:text_colors:failed";
        break;
    case eTaskStateCompleted:
        state_texture = "ui_icons_PDA_subtask_accomplished";
        color_path = "task_sub_item:description:text_colors:accomplished";
        break;
    default:
        state_texture = "ui_icons_PDA_subtask_active";
        color_path = "task_sub_item:description:text_colors:active";
        break;
    }

    if (m_state_image)
        m_state_image->InitTexture(state_texture, false);
    if (m_description && xml.NavigateToNode(color_path))
        m_description->SetTextColor(CUIXmlInit::GetColor(xml, color_path, 0, m_description->GetTextColor()));
}

bool CUISHoCTaskItem::IsActiveObjective() const
{
    const CGameTask* active = Level().GameTaskManager().ActiveTask(m_task->GetTaskType());
    return active == m_task && m_task->ActiveObjectiveIdx() == m_objective;
}

void CUISHoCTaskItem::Update()
{
    inherited::Update();

    if (m_active_objective_image)
        m_active_objective_image->Show(IsActiveObjective());
    if (m_show_descr_btn)
        m_show_descr_btn->Show(m_owner->ItemHasDescription(this));
    if (m_switch_description_btn)
        m_switch_description_btn->InitTexture(m_owner->IsMapMode() ? TEX_SHOW_TEXT : TEX_SHOW_MAP, false);

    if (m_rem_time && m_rem_time->IsShown())
    {
        string256 period;
        InventoryUtilities::GetTimePeriodAsString(period, sizeof(period), Level().GetGameTime(), m_task->m_TimeToComplete);
        string512 buff;
        xr_sprintf(buff, "%s %s", StringTable().translate("ui_st_time_remains").c_str(), period);
        m_rem_time->SetText(buff);
    }
}

void CUISHoCTaskItem::MarkSelected(bool b)
{
    if (m_show_descr_btn)
        m_show_descr_btn->SetButtonState(b ? CUIButton::BUTTON_PUSHED : CUIButton::BUTTON_NORMAL);
}

bool CUISHoCTaskItem::OnMouseDown(int mouse_btn)
{
    // Let the buttons of the item receive their clicks
    if (IsCursorOver(m_switch_description_btn, cursor_pos) || IsCursorOver(m_show_descr_btn, cursor_pos))
        return false;

    if (mouse_btn == MOUSE_1)
    {
        m_owner->ShowDescription(m_task, m_objective);
        return true;
    }
    return false;
}

bool CUISHoCTaskItem::OnDbClick()
{
    // Double click on an objective in progress makes it the active one
    if (m_objective != ROOT_TASK_OBJECTIVE && m_task->Objective(m_objective).GetTaskState() == eTaskStateInProgress)
        Level().GameTaskManager().SetActiveTask(m_task, m_objective);
    return true;
}

void CUISHoCTaskItem::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (msg == BUTTON_CLICKED)
    {
        if (pWnd == m_switch_description_btn)
        {
            m_owner->SetMapMode(!m_owner->IsMapMode());
            m_owner->ShowDescription(m_task, m_objective);
            return;
        }
        if (pWnd == m_show_descr_btn)
        {
            m_owner->ShowDescription(m_task, m_objective);
            return;
        }
    }
    inherited::SendMessage(pWnd, msg, pData);
}

//////////////////////////////////////////////////////////////////////////
// CUISHoCTaskWnd

CUISHoCTaskWnd::CUISHoCTaskWnd(UIHint* hint) : CUIWindow("CUISHoCTaskWnd"), hint_wnd(hint) {}

CUISHoCTaskWnd::~CUISHoCTaskWnd()
{
    // Only one of them is attached at a time, so they are owned here
    delete_data(m_map_wnd);
    delete_data(m_descr_wnd);
}

bool CUISHoCTaskWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_TASK_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    // Left part: filter and task list
    CUIWindow* left = this;
    if (CUIFrameWindow* frame = UIHelper::CreateFrameWindow(xml, "main_wnd:left_frame", this, false))
    {
        left = frame;
        CUIXmlInit::InitAutoStaticGroup(xml, "main_wnd:left_frame", 0, frame);
    }

    CUIWindow* left_header = UIHelper::CreateFrameLine(xml, "main_wnd:left_frame:left_frame_header", left, false);
    constexpr pcstr anim_path = "main_wnd:left_frame:left_frame_header:anim_static";
    if (xml.NavigateToNode(anim_path))
    {
        auto* anim = xr_new<CUIAnimatedStatic>();
        anim->SetAutoDelete(true);
        (left_header ? left_header : left)->AttachChild(anim);
        CUIXmlInit::InitAnimatedStatic(xml, anim_path, 0, anim);
    }

    m_list = UIHelper::CreateScrollView(xml, "main_wnd:left_frame:list", left, false);
    if (!m_list)
        return false;

    constexpr pcstr filter_path = "main_wnd:left_frame:filter_tab";
    if (xml.NavigateToNode(filter_path))
    {
        m_filter_tab = xr_new<CUITabControl>();
        m_filter_tab->SetAutoDelete(true);
        left->AttachChild(m_filter_tab);
        CUIXmlInit::InitTabControl(xml, filter_path, 0, m_filter_tab, false, true);
        m_filter_tab->SetMessageTarget(this);
        m_filter_tab->SetActiveTabByIndex(0);
    }

    // Right part: map or task description
    m_right_wnd = UIHelper::CreateNormalWindow(xml, "main_wnd:right_frame", this, false);
    if (!m_right_wnd)
        m_right_wnd = this;

    constexpr pcstr descr_path = "main_wnd:right_frame:task_descr_view";
    if (xml.NavigateToNode(descr_path))
    {
        m_descr_wnd = xr_new<CUIWindow>("task_descr_view");
        CUIXmlInit::InitWindow(xml, descr_path, 0, m_descr_wnd);

        CUIWindow* descr_frame = UIHelper::CreateFrameWindow(xml, "main_wnd:right_frame:task_descr_view:main_frame", m_descr_wnd, false);
        if (!descr_frame)
            descr_frame = m_descr_wnd;
        CUIXmlInit::InitAutoStaticGroup(xml, "main_wnd:right_frame:task_descr_view:main_frame", 0, descr_frame);
        UIHelper::CreateFrameLine(xml, "main_wnd:right_frame:task_descr_view:main_frame:header_frame_line", descr_frame, false);
        m_descr_view = UIHelper::CreateScrollView(xml, "main_wnd:right_frame:task_descr_view:main_frame:scroll_view", descr_frame, false);
    }

    m_map_wnd = xr_new<CUIMapWnd>(hint_wnd);
    if (!m_map_wnd->Init(SHOC_TASK_XML, "main_wnd:right_frame:map_wnd", false))
        xr_delete(m_map_wnd);

    // Only attach here: showing the map before the level is loaded crashes in CUIMapWnd::Activated()
    m_map_mode = m_map_wnd != nullptr;
    if (CUIWindow* right = m_map_mode ? static_cast<CUIWindow*>(m_map_wnd) : m_descr_wnd)
        m_right_wnd->AttachChild(right);

    return true;
}

void CUISHoCTaskWnd::Show(bool status)
{
    inherited::Show(status);

    if (m_map_wnd)
        m_map_wnd->Show(status && m_map_mode);
    if (m_descr_wnd)
        m_descr_wnd->Show(status && !m_map_mode);

    if (status)
        ReloadList();
    else
        m_list->Clear();
}

void CUISHoCTaskWnd::Update()
{
    if (TasksSignature() != m_tasks_signature)
        ReloadList();

    inherited::Update();
}

void CUISHoCTaskWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (pWnd == m_filter_tab && msg == TAB_CHANGED)
    {
        const int index = m_filter_tab->GetActiveIndex();
        m_filter = index == int(EFilter::Accomplished) ? EFilter::Accomplished
            : index == int(EFilter::Failed)            ? EFilter::Failed
                                                       : EFilter::Active;
        ReloadList();
        if (!m_map_mode && m_map_wnd)
            SetMapMode(true);
        return;
    }

    inherited::SendMessage(pWnd, msg, pData);
}

bool CUISHoCTaskWnd::Filter(CGameTask* task) const
{
    switch (m_filter)
    {
    case EFilter::Accomplished: return task->GetTaskState() == eTaskStateCompleted;
    case EFilter::Failed: return task->GetTaskState() == eTaskStateFail;
    default: return task->GetTaskState() == eTaskStateInProgress;
    }
}

u64 CUISHoCTaskWnd::TasksSignature()
{
    if (!g_pGameLevel)
        return 0;

    // Changes whenever a task is added or a task/objective changes its state
    u64 signature = 1469598103934665603ull;
    const auto mix = [&signature](u64 value) { signature = (signature ^ value) * 1099511628211ull; };

    for (const SGameTaskKey& key : Level().GameTaskManager().GetGameTasks())
    {
        CGameTask* task = key.game_task;
        if (!task)
            continue;
        mix(task->GetObjectivesCount());
        for (TASK_OBJECTIVE_ID i = 0; i < task->GetObjectivesCount(); ++i)
            mix(task->Objective(i).GetTaskState());
    }
    return signature;
}

void CUISHoCTaskWnd::ReloadList()
{
    m_list->Clear();
    m_tasks_signature = TasksSignature();

    if (!g_pGameLevel)
        return;

    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_TASK_ITEM_XML, false))
        return;

    for (const SGameTaskKey& key : Level().GameTaskManager().GetGameTasks())
    {
        CGameTask* task = key.game_task;
        if (!task || !Filter(task))
            continue;

        for (TASK_OBJECTIVE_ID i = 0; i < task->GetObjectivesCount(); ++i)
        {
            auto* item = xr_new<CUISHoCTaskItem>(this, task, i);
            item->Init(xml);
            m_list->AddWindow(item, true);
        }
    }
}

void CUISHoCTaskWnd::SetMapMode(bool map_mode)
{
    if (!m_map_wnd)
        map_mode = false;

    m_map_mode = map_mode;

    CUIWindow* show = map_mode ? static_cast<CUIWindow*>(m_map_wnd) : m_descr_wnd;
    CUIWindow* hide = map_mode ? m_descr_wnd : static_cast<CUIWindow*>(m_map_wnd);

    if (hide && m_right_wnd->IsChild(hide))
    {
        m_right_wnd->DetachChild(hide);
        hide->Show(false);
    }
    if (show && !m_right_wnd->IsChild(show))
    {
        m_right_wnd->AttachChild(show);
        show->Show(IsShown());
    }
}

void CUISHoCTaskWnd::ShowDescription(CGameTask* task, TASK_OBJECTIVE_ID objective)
{
    if (m_map_mode)
    {
        CMapLocation* ml = task->Objective(objective).LinkedMapLocation();
        if (ml && ml->SpotEnabled())
            m_map_wnd->SetTargetMap(ml->GetLevelName(), ml->GetPosition(), true);
    }
    else
    {
        objective = ROOT_TASK_OBJECTIVE; // articles are always shown for the whole task
        ShowArticles(task, objective);
    }

    for (CUIWindow* w : m_list->Items())
    {
        if (auto* item = smart_cast<CUISHoCTaskItem*>(w))
            item->MarkSelected(item->GameTask() == task && item->ObjectiveIdx() == objective);
    }
}

void CUISHoCTaskWnd::ShowArticles(CGameTask* task, TASK_OBJECTIVE_ID objective)
{
    if (!m_descr_view)
        return;

    m_descr_view->Clear();

    if (!Actor() || !Actor()->encyclopedia_registry)
        return;

    // Task articles belong to the group "<task id>" or "<task id>/<something>"
    const shared_str& need_group = task->m_ID;
    const u32 need_len = need_group.size();
    const shared_str& article_id = task->Objective(objective).m_article_id;

    for (const ARTICLE_DATA& data : Actor()->encyclopedia_registry->registry().objects())
    {
        if (data.article_type != ARTICLE_DATA::eTaskArticle)
            continue;

        CEncyclopediaArticle article;
        article.Load(data.article_id);
        const shared_str& group = article.data()->group;

        const bool in_group = need_len && group.size() >= need_len &&
            0 == strncmp(group.c_str(), need_group.c_str(), need_len) &&
            (group.size() == need_len || group.c_str()[need_len] == '/');

        if (in_group || (article_id.size() && data.article_id == article_id))
            CUIArticleBrowser::AddArticleView(m_descr_view, data.article_id, DESCR_ITEM_PATH);
    }
}

bool CUISHoCTaskWnd::ItemHasDescription(const CUISHoCTaskItem* item) const
{
    CMapLocation* ml = item->GameTask()->Objective(item->ObjectiveIdx()).LinkedMapLocation();
    if (item->ObjectiveIdx() == ROOT_TASK_OBJECTIVE)
        return ml != nullptr;
    return m_map_mode && ml && ml->SpotEnabled();
}
