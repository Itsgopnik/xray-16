#include "pch_script.h"
#include "UISHoCTaskWnd.h"

#include "UIXmlInit.h"
#include "UIMapWnd.h"
#include "UIEncyclopediaArticleWnd.h"
#include "UIInventoryUtilities.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/Buttons/UI3tButton.h"
#include "xrUICore/ListWnd/UIListItem.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/TabControl/UITabControl.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrEngine/StringTable/StringTable.h"
#include "Actor.h"
#include "Level.h"
#include "GametaskManager.h"
#include "GameTask.h"
#include "map_location.h"
#include "encyclopedia_article.h"
#include "alife_registry_wrappers.h"

#define SHOC_TASK_XML "pda_events.xml"
#define SHOC_TASK_ITEM_XML "job_item.xml"
#define SHOC_ARTICLE_XML "encyclopedia_item.xml"

namespace
{
template <typename T, typename... Args>
T* create(CUIWindow* parent, Args&&... args)
{
    T* wnd = xr_new<T>(std::forward<Args>(args)...);
    wnd->SetAutoDelete(true);
    parent->AttachChild(wnd);
    return wnd;
}

bool is_active_objective(CGameTask* task, u16 objective)
{
    return Level().GameTaskManager().ActiveTask() == task && task->ActiveObjectiveIdx() == objective;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Task articles view (task_descr_view)

class CUISHoCTaskDescrWnd final : public CUIWindow
{
    CUIScrollView* m_view{};

public:
    CUISHoCTaskDescrWnd() : CUIWindow("CUISHoCTaskDescrWnd") {}

    void Init(CUIXml& xml, pcstr start_from)
    {
        string512 path;
        CUIXmlInit::InitWindow(xml, start_from, 0, this);

        auto* frame = create<CUIFrameWindow>(this, "main_frame");
        strconcat(path, start_from, ":main_frame");
        CUIXmlInit::InitFrameWindow(xml, path, 0, frame);
        CUIXmlInit::InitAutoStaticGroup(xml, path, 0, frame);

        auto* header = create<CUIFrameLineWnd>(frame, "header_frame_line");
        strconcat(path, start_from, ":main_frame:header_frame_line");
        CUIXmlInit::InitFrameLine(xml, path, 0, header);

        m_view = create<CUIScrollView>(frame);
        strconcat(path, start_from, ":main_frame:scroll_view");
        CUIXmlInit::InitScrollView(xml, path, 0, m_view);
    }

    void ClearAll() { m_view->Clear(); }

    void AddArticle(CEncyclopediaArticle* article)
    {
        auto* wnd = xr_new<CUIEncyclopediaArticleWnd>();
        wnd->Init(SHOC_ARTICLE_XML, "events_wnd:objective_item");
        wnd->SetArticle(article);
        m_view->AddWindow(wnd, true);
    }

    pcstr GetDebugType() override { return "CUISHoCTaskDescrWnd"; }
};

//////////////////////////////////////////////////////////////////////////
// List items (job_item.xml)

class CUISHoCTaskItem : public CUIListItem, public CUIWndCallback
{
protected:
    CUISHoCTaskWnd* m_owner;
    CGameTask* m_task{};
    u16 m_objective{};

    template <typename T>
    T* add(pcstr node)
    {
        string256 path;
        strconcat(path, m_node, ":", node);
        if constexpr (std::is_same_v<T, CUI3tButton>)
        {
            T* wnd = create<T>(this);
            CUIXmlInit::Init3tButton(m_owner->m_item_xml, path, 0, wnd);
            return wnd;
        }
        else
        {
            T* wnd = create<T>(this, node);
            CUIXmlInit::InitStatic(m_owner->m_item_xml, path, 0, wnd);
            return wnd;
        }
    }

    void OnItemClicked(CUIWindow*, void*) { m_owner->ShowDescription(m_task, m_objective); }

private:
    pcstr m_node;

public:
    CUISHoCTaskItem(CUISHoCTaskWnd* owner, pcstr node) : m_owner(owner), m_node(node)
    {
        // The callbacks below go to this item, not to the list
        Register(this);
        AddCallback(this, BUTTON_CLICKED, CUIWndCallback::void_function(this, &CUISHoCTaskItem::OnItemClicked));
        CUIXmlInit::InitWindow(owner->m_item_xml, node, 0, this);
    }

    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override { CUIWndCallback::OnEvent(pWnd, msg, pData); }

    virtual void SetGameTask(CGameTask* task, u16 objective)
    {
        m_task = task;
        m_objective = objective;
    }

    CGameTask* GameTask() const { return m_task; }
    u16 ObjectiveIdx() const { return m_objective; }
    SGameTaskObjective& Objective() const { return m_task->Objective(m_objective); }

    pcstr GetDebugType() override { return "CUISHoCTaskItem"; }
};

class CUISHoCTaskRootItem final : public CUISHoCTaskItem
{
    CUIStatic* m_image;
    CUIStatic* m_caption;
    CUIStatic* m_caption_time;
    CUIStatic* m_rem_time;
    CUI3tButton* m_switch_descr_btn;
    bool m_curr_descr_mode{};

    void UpdateSwitchTexture()
    {
        m_curr_descr_mode = m_owner->GetDescriptionMode();
        m_switch_descr_btn->InitTexture(m_curr_descr_mode ? "ui_icons_newPDA_showtext" : "ui_icons_newPDA_showmap");
    }

    void OnSwitchDescriptionClicked(CUIWindow*, void*)
    {
        m_owner->SetDescriptionMode(!m_owner->GetDescriptionMode());
        OnItemClicked(this, nullptr);
    }

public:
    CUISHoCTaskRootItem(CUISHoCTaskWnd* owner) : CUISHoCTaskItem(owner, "task_root_item")
    {
        m_image = add<CUIStatic>("image");
        m_caption = add<CUIStatic>("caption");
        m_caption_time = add<CUIStatic>("caption_time");
        m_rem_time = add<CUIStatic>("rem_time");
        m_switch_descr_btn = add<CUI3tButton>("switch_description_btn");
        Register(m_switch_descr_btn);
        AddCallback(m_switch_descr_btn, BUTTON_CLICKED,
            CUIWndCallback::void_function(this, &CUISHoCTaskRootItem::OnSwitchDescriptionClicked));
    }

    void SetGameTask(CGameTask* task, u16 objective) override
    {
        CUISHoCTaskItem::SetGameTask(task, objective);
        const SGameTaskObjective& obj = Objective();

        if (obj.m_icon_texture_name.size())
        {
            // Returns false for plain texture files (not atlas ids) but loads them anyway
            m_image->InitTexture(obj.m_icon_texture_name.c_str(), false);
            // SoC rects are x, y, width, height
            const Frect& r = obj.m_icon_rect;
            m_image->SetTextureRect({ r.x1, r.y1, r.x1 + r.x2, r.y1 + r.y2 });
            m_image->SetStretchTexture(true);
        }

        m_caption->SetTextST(task->m_Title.c_str());
        m_caption->AdjustHeightToText();

        xr_string time = InventoryUtilities::GetDateAsString(task->m_ReceiveTime, InventoryUtilities::edpDateToDay, '/').c_str();
        time += " ";
        time += InventoryUtilities::GetTimeAsString(task->m_ReceiveTime, InventoryUtilities::etpTimeToMinutes).c_str();
        m_caption_time->SetText(time.c_str());
        m_caption_time->SetWndPos({ m_caption_time->GetWndPos().x, m_caption->GetWndPos().y + m_caption->GetHeight() + 3.0f });

        float h = _max(m_image->GetWndPos().y + m_image->GetHeight(), m_caption_time->GetWndPos().y + m_caption_time->GetHeight());
        h = _max(h, m_switch_descr_btn->GetWndPos().y + m_switch_descr_btn->GetHeight());
        SetHeight(h);

        UpdateSwitchTexture();

        m_rem_time->Show(task->GetTaskState() == eTaskStateInProgress && task->m_ReceiveTime != task->m_TimeToComplete);
        if (m_rem_time->IsShown())
        {
            Fvector2 pos = m_caption_time->GetWndPos();
            pos.y += m_caption_time->GetHeight();
            pos.x = m_rem_time->GetWndPos().x;
            m_rem_time->SetWndPos(pos);
            SetHeight(_max(GetHeight(), pos.y + m_rem_time->GetHeight()));
        }
    }

    void Update() override
    {
        CUISHoCTaskItem::Update();
        if (m_curr_descr_mode != m_owner->GetDescriptionMode())
            UpdateSwitchTexture();

        if (m_rem_time->IsShown())
        {
            string512 period, text;
            InventoryUtilities::GetTimePeriodAsString(period, sizeof(period), Level().GetGameTime(), m_task->m_TimeToComplete);
            xr_sprintf(text, "%s %s", StringTable().translate("ui_st_time_remains").c_str(), period);
            m_rem_time->SetText(text);
        }
    }

    bool OnDbClick() override { return true; }
};

class CUISHoCTaskSubItem final : public CUISHoCTaskItem
{
    CUIStatic* m_state_image;
    CUIStatic* m_description;
    CUIStatic* m_active_image;
    CUI3tButton* m_show_descr_btn;
    u32 m_active_color;
    u32 m_failed_color;
    u32 m_accomplished_color;

public:
    CUISHoCTaskSubItem(CUISHoCTaskWnd* owner) : CUISHoCTaskItem(owner, "task_sub_item")
    {
        m_state_image = add<CUIStatic>("state_image");
        m_description = add<CUIStatic>("description");
        m_active_image = add<CUIStatic>("active_objecttive_image");
        m_show_descr_btn = add<CUI3tButton>("show_descr_btn");
        Register(m_show_descr_btn);
        AddCallback(m_show_descr_btn, BUTTON_CLICKED, CUIWndCallback::void_function(this, &CUISHoCTaskSubItem::OnItemClicked));

        CUIXml& xml = owner->m_item_xml;
        m_active_color = CUIXmlInit::GetColor(xml, "task_sub_item:description:text_colors:active", 0, 0x00);
        m_failed_color = CUIXmlInit::GetColor(xml, "task_sub_item:description:text_colors:failed", 0, 0x00);
        m_accomplished_color = CUIXmlInit::GetColor(xml, "task_sub_item:description:text_colors:accomplished", 0, 0x00);
    }

    void SetGameTask(CGameTask* task, u16 objective) override
    {
        CUISHoCTaskItem::SetGameTask(task, objective);
        const SGameTaskObjective& obj = Objective();

        m_description->SetTextST(obj.m_Description.c_str());
        m_description->AdjustHeightToText();
        SetHeight(_max(m_active_image->GetWndPos().y + m_active_image->GetHeight(),
            m_description->GetWndPos().y + m_description->GetHeight()));

        switch (obj.GetTaskState())
        {
        case eTaskStateInProgress:
            m_state_image->InitTexture("ui_icons_PDA_subtask_active");
            m_description->SetTextColor(m_active_color);
            break;
        case eTaskStateFail:
            m_state_image->InitTexture("ui_icons_PDA_subtask_failed");
            m_description->SetTextColor(m_failed_color);
            break;
        case eTaskStateCompleted:
            m_state_image->InitTexture("ui_icons_PDA_subtask_accomplished");
            m_description->SetTextColor(m_accomplished_color);
            break;
        default: break;
        }
    }

    void Update() override
    {
        CUISHoCTaskItem::Update();
        m_active_image->Show(is_active_objective(m_task, m_objective));
        m_show_descr_btn->Show(m_owner->ItemHasDescription(this));
    }

    // Makes an objective in progress the active one
    bool OnDbClick() override
    {
        if (Objective().GetTaskState() == eTaskStateInProgress && !is_active_objective(m_task, m_objective))
            Level().GameTaskManager().SetActiveTask(m_task, m_objective);
        return true;
    }

    void MarkSelected(bool b) override
    {
        m_show_descr_btn->SetButtonState(b ? CUIButton::BUTTON_PUSHED : CUIButton::BUTTON_NORMAL);
    }
};

//////////////////////////////////////////////////////////////////////////
// CUISHoCTaskWnd

CUISHoCTaskWnd::CUISHoCTaskWnd(UIHint* hint) : CUIWindow("CUISHoCTaskWnd"), hint_wnd(hint) {}

CUISHoCTaskWnd::~CUISHoCTaskWnd()
{
    // Not auto-deleted: only one of them is attached at a time
    delete_data(m_map_wnd);
    delete_data(m_descr_wnd);
}

bool CUISHoCTaskWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_TASK_XML, false))
        return false;
    if (!m_item_xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_TASK_ITEM_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    auto* left_frame = create<CUIFrameWindow>(this, "left_frame");
    CUIXmlInit::InitFrameWindow(xml, "main_wnd:left_frame", 0, left_frame);
    CUIXmlInit::InitAutoStaticGroup(xml, "main_wnd:left_frame", 0, left_frame);

    auto* left_header = create<CUIFrameLineWnd>(left_frame, "left_frame_header");
    CUIXmlInit::InitFrameLine(xml, "main_wnd:left_frame:left_frame_header", 0, left_header);

    if (xml.NavigateToNode("main_wnd:left_frame:left_frame_header:anim_static", 0))
    {
        auto* anim = create<CUIAnimatedStatic>(left_header);
        CUIXmlInit::InitAnimatedStatic(xml, "main_wnd:left_frame:left_frame_header:anim_static", 0, anim);
    }

    m_right_wnd = create<CUIWindow>(this, "right_frame");
    CUIXmlInit::InitWindow(xml, "main_wnd:right_frame", 0, m_right_wnd);

    m_map_wnd = xr_new<CUIMapWnd>(hint_wnd);
    m_map_wnd->SetAutoDelete(false);
    if (!m_map_wnd->Init(SHOC_TASK_XML, "main_wnd:right_frame:map_wnd", false))
        return false;

    m_descr_wnd = xr_new<CUISHoCTaskDescrWnd>();
    m_descr_wnd->SetAutoDelete(false);
    m_descr_wnd->Init(xml, "main_wnd:right_frame:task_descr_view");

    m_list = create<CUIScrollView>(left_frame);
    CUIXmlInit::InitScrollView(xml, "main_wnd:left_frame:list", 0, m_list);

    m_filter_tab = create<CUITabControl>(left_frame);
    CUIXmlInit::InitTabControl(xml, "main_wnd:left_frame:filter_tab", 0, m_filter_tab, true, true);
    Register(m_filter_tab);
    AddCallback(m_filter_tab, TAB_CHANGED, CUIWndCallback::void_function(this, &CUISHoCTaskWnd::OnFilterChanged));

    SetDescriptionMode(true);
    return true;
}

void CUISHoCTaskWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData) { CUIWndCallback::OnEvent(pWnd, msg, pData); }

void CUISHoCTaskWnd::Show(bool status)
{
    if (status && m_filter_tab->GetActiveIndex() < 0)
        m_filter_tab->SetActiveTabByIndex(eActiveTask);

    inherited::Show(status);

    // The map has to be attached while shown, or its level maps disappear (noted in OGSR too)
    if (!m_map_mode)
        m_right_wnd->AttachChild(m_map_wnd);
    m_map_wnd->Show(status);
    m_descr_wnd->Show(status);
    if (!m_map_mode)
        m_right_wnd->DetachChild(m_map_wnd);

    if (status)
        ReloadList();
    else
        m_list->Clear();
}

void CUISHoCTaskWnd::Update()
{
    if (m_need_reload && IsShown())
        ReloadList();
    inherited::Update();
}

void CUISHoCTaskWnd::Reset()
{
    inherited::Reset();
    m_map_wnd->Reset();
    if (IsShown())
        ReloadList();
}

void CUISHoCTaskWnd::DrawHint()
{
    if (m_map_mode)
        m_map_wnd->DrawHint();
}

void CUISHoCTaskWnd::OnFilterChanged(CUIWindow*, void*)
{
    m_filter = static_cast<ETaskFilter>(m_filter_tab->GetActiveIndex());
    ReloadList();
    if (!m_map_mode)
        SetDescriptionMode(true);
}

bool CUISHoCTaskWnd::Filter(CGameTask* task) const
{
    switch (task->GetTaskState())
    {
    case eTaskStateInProgress: return m_filter == eActiveTask;
    case eTaskStateCompleted: return m_filter == eAccomplishedTask;
    case eTaskStateFail: return m_filter == eFailedTask;
    default: return false;
    }
}

void CUISHoCTaskWnd::ReloadList()
{
    m_need_reload = false;
    m_list->Clear();
    if (!g_pGameLevel || !Actor())
        return;

    xr_vector<CGameTask*> tasks;
    for (const SGameTaskKey& key : Level().GameTaskManager().GetGameTasks())
    {
        if (key.game_task && Filter(key.game_task))
            tasks.push_back(key.game_task);
    }

    if (m_filter == eActiveTask)
    {
        std::stable_sort(tasks.begin(), tasks.end(), [](const CGameTask* a, const CGameTask* b) {
            if (a->m_priority == b->m_priority)
                return a->m_ReceiveTime > b->m_ReceiveTime;
            return a->m_priority < b->m_priority;
        });
    }

    for (CGameTask* task : tasks)
    {
        const u16 count = task->GetObjectivesCount();
        for (u16 i = 0; i < count; ++i)
        {
            CUISHoCTaskItem* item = i == ROOT_TASK_OBJECTIVE ?
                static_cast<CUISHoCTaskItem*>(xr_new<CUISHoCTaskRootItem>(this)) :
                static_cast<CUISHoCTaskItem*>(xr_new<CUISHoCTaskSubItem>(this));
            item->SetGameTask(task, i);
            m_list->AddWindow(item, true);
        }
    }
}

void CUISHoCTaskWnd::SetDescriptionMode(bool map)
{
    CUIWindow* shown = map ? static_cast<CUIWindow*>(m_map_wnd) : m_descr_wnd;
    CUIWindow* hidden = map ? static_cast<CUIWindow*>(m_descr_wnd) : m_map_wnd;
    if (m_right_wnd->IsChild(hidden))
        m_right_wnd->DetachChild(hidden);
    if (!m_right_wnd->IsChild(shown))
        m_right_wnd->AttachChild(shown);
    m_map_mode = map;
}

void CUISHoCTaskWnd::ShowDescription(CGameTask* task, u16 objective)
{
    if (m_map_mode)
    {
        CMapLocation* ml = task->Objective(objective).LinkedMapLocation();
        if (ml && ml->SpotEnabled())
            m_map_wnd->SetTargetMap(ml->GetLevelName(), ml->GetPosition(), true);

        for (CUIWindow* wnd : m_list->Items())
        {
            auto* item = static_cast<CUISHoCTaskItem*>(wnd);
            item->MarkSelected(item->GameTask() == task && item->ObjectiveIdx() == objective);
        }
        return;
    }

    // Task articles: those of the task's group and the objective's own
    m_descr_wnd->ClearAll();
    const SGameTaskObjective& root = task->Objective(ROOT_TASK_OBJECTIVE);
    bool root_article_shown = false;
    for (const ARTICLE_DATA& data : Actor()->encyclopedia_registry->registry().objects())
    {
        if (data.article_type != ARTICLE_DATA::eTaskArticle)
            continue;
        CEncyclopediaArticle article;
        article.Load(data.article_id);
        const bool is_root_article = root.m_article_id.size() && data.article_id == root.m_article_id;
        if (task->m_ID == article.data()->group || is_root_article)
        {
            m_descr_wnd->AddArticle(&article);
            root_article_shown |= is_root_article;
        }
    }

    // Tasks received before the task's own article was registered (older saves)
    if (!root_article_shown && root.m_article_id.size())
    {
        CEncyclopediaArticle article;
        article.Load(root.m_article_id);
        m_descr_wnd->AddArticle(&article);
    }
}

bool CUISHoCTaskWnd::ItemHasDescription(CUISHoCTaskItem* item) const
{
    CGameTask* task = item->GameTask();
    if (item->ObjectiveIdx() == ROOT_TASK_OBJECTIVE)
    {
        for (u16 i = 1; i < task->GetObjectivesCount(); ++i)
        {
            if (task->Objective(i).LinkedMapLocation())
                return true;
        }
        return false;
    }
    CMapLocation* ml = item->Objective().LinkedMapLocation();
    return m_map_mode && ml && ml->SpotEnabled();
}
