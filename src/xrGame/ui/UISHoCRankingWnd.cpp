#include "pch_script.h"
#include "UISHoCRankingWnd.h"

#include "UIXmlInit.h"
#include "UIHelper.h"
#include "UICharacterInfo.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "Actor.h"
#include "ai_space.h"
#include "alife_simulator.h"
#include "alife_object_registry.h"
#include "xrServerEntities/xrServer_Objects_ALife_Monsters.h"

#define SHOC_RANKING_XML "stalkers_ranking.xml"
#define SHOC_RANKING_CHARACTER_XML "stalkers_ranking_character.xml"

CSE_ALifeTraderAbstract* ch_info_get_from_id(u16 id);

namespace
{
constexpr int TOP_LIST_SIZE = 20;
}

CUISHoCRankingItem::CUISHoCRankingItem(CUISHoCRankingWnd* owner, bool selectable)
    : CUIWindow("CUISHoCRankingItem"), m_owner(owner), m_selectable(selectable) {}

void CUISHoCRankingItem::Init(CUIXml& xml, pcstr path)
{
    CUIXmlInit::InitWindow(xml, path, 0, this);

    const XML_NODE stored_root = xml.GetLocalRoot();
    xml.SetLocalRoot(xml.NavigateToNode(path, 0));

    m_text1 = UIHelper::CreateStatic(xml, "text_1", this, false);
    m_text2 = UIHelper::CreateStatic(xml, "text_2", this, false);
    m_text3 = UIHelper::CreateStatic(xml, "text_3", this, false);
    CUIXmlInit::InitAutoStaticGroup(xml, "auto", 0, this);

    if (m_text2)
        m_stored_alpha = color_get_A(m_text2->GetTextColor());

    xml.SetLocalRoot(stored_root);
}

void CUISHoCRankingItem::SetSelected(bool b)
{
    if (!m_selectable)
        return;

    CUISelectable::SetSelected(b);

    for (CUIStatic* text : { m_text1, m_text2, m_text3 })
    {
        if (text)
            text->SetTextColor(subst_alpha(text->GetTextColor(), b ? 255 : m_stored_alpha));
    }

    if (b)
        m_owner->ShowHumanInfo(m_humanID);
}

bool CUISHoCRankingItem::OnMouseDown(int mouse_btn)
{
    if (mouse_btn == MOUSE_1 && m_selectable)
    {
        m_owner->SelectItem(this);
        return true;
    }
    return false;
}

CUISHoCRankingWnd::CUISHoCRankingWnd() : CUIWindow("CUISHoCRankingWnd") {}

bool CUISHoCRankingWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_RANKING_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    // Right part: selected character
    CUIWindow* char_parent = this;
    if (CUIFrameWindow* frame = UIHelper::CreateFrameWindow(xml, "chicon_frame_window", this, false))
    {
        char_parent = frame;
        UIHelper::CreateFrameLine(xml, "chicon_frame_line", frame, false);
    }

    // Left part: top list
    CUIWindow* list_parent = this;
    CUIWindow* header = nullptr;
    if (CUIFrameWindow* frame = UIHelper::CreateFrameWindow(xml, "info_frame_window", this, false))
    {
        list_parent = frame;
        header = UIHelper::CreateFrameLine(xml, "info_frame_line", frame, false);
    }

    if (xml.NavigateToNode("a_static"))
    {
        auto* anim = xr_new<CUIAnimatedStatic>();
        anim->SetAutoDelete(true);
        (header ? header : list_parent)->AttachChild(anim);
        CUIXmlInit::InitAnimatedStatic(xml, "a_static", 0, anim);
    }

    m_list = UIHelper::CreateScrollView(xml, "list", list_parent, false);
    if (!m_list)
        return false;

    if (xml.NavigateToNode("character_info"))
    {
        auto* char_wnd = xr_new<CUIWindow>("character_info");
        char_wnd->SetAutoDelete(true);
        char_parent->AttachChild(char_wnd);
        CUIXmlInit::InitWindow(xml, "character_info", 0, char_wnd);

        CUIXml char_xml;
        if (char_xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_RANKING_CHARACTER_XML, false))
        {
            m_character_info = xr_new<CUICharacterInfo>();
            m_character_info->SetAutoDelete(true);
            char_wnd->AttachChild(m_character_info);
            m_character_info->InitCharacterInfo({ 0.0f, 0.0f }, char_wnd->GetWndSize(), &char_xml);
        }
    }

    CUIXmlInit::InitAutoStaticGroup(xml, "left_auto", 0, list_parent);
    CUIXmlInit::InitAutoStaticGroup(xml, "right_auto", 0, char_parent);

    return true;
}

void CUISHoCRankingWnd::Show(bool status)
{
    if (status)
        FillList();
    inherited::Show(status);
}

void CUISHoCRankingWnd::FillList()
{
    m_list->Clear();
    if (m_character_info)
        m_character_info->ClearInfo();

    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_RANKING_XML, false))
        return;

    const XML_NODE list_node = xml.NavigateToNode("stalkers_list", 0);
    if (!list_node)
        return;
    xml.SetLocalRoot(list_node);

    struct Entry
    {
        u16 id;
        CHARACTER_RANK_VALUE rank;
    };
    xr_vector<Entry> entries;

    if (ai().get_alife())
    {
        for (const auto& [id, object] : ai().alife().objects().objects())
        {
            auto* stalker = smart_cast<CSE_ALifeHumanStalker*>(object);
            if (stalker && stalker->g_Alive())
                entries.push_back({ id, stalker->CSE_ALifeTraderAbstract::Rank() });
        }
    }

    const u16 actor_id = Actor() ? Actor()->ID() : u16(-1);
    if (actor_id != u16(-1))
    {
        if (CSE_ALifeTraderAbstract* actor = ch_info_get_from_id(actor_id))
            entries.push_back({ actor_id, actor->Rank() });
    }

    if (entries.empty())
    {
        AddItem(xml, "no_items", 0, u16(-1), false);
        return;
    }

    std::stable_sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.rank > b.rank; });

    int actor_place = -1;
    for (int i = 0; i < int(entries.size()); ++i)
    {
        if (entries[i].id == actor_id)
        {
            actor_place = i;
            break;
        }
    }

    // The actor is always shown: in its own place, or instead of the last
    // entry (preceded by "...") when it is not in the top list.
    const int count = std::min(int(entries.size()), TOP_LIST_SIZE);
    for (int i = 0; i < count; ++i)
    {
        const bool actor_out_of_top = i == count - 1 && actor_place >= TOP_LIST_SIZE;
        if (entries[i].id == actor_id || actor_out_of_top)
        {
            if (actor_out_of_top)
                AddItem(xml, "item_ellipsis", 0, u16(-1), false);
            AddItem(xml, "item_actor", actor_place + 1, actor_id, true);
        }
        else
            AddItem(xml, "item_human", i + 1, entries[i].id, true);
    }

    SelectItem(m_list->GetItem(0));
}

void CUISHoCRankingWnd::AddItem(CUIXml& xml, pcstr path, int place, u16 id, bool selectable)
{
    auto* item = xr_new<CUISHoCRankingItem>(this, selectable);
    item->Init(xml, path);
    item->m_humanID = id;

    if (id != u16(-1))
    {
        if (CSE_ALifeTraderAbstract* trader = ch_info_get_from_id(id))
        {
            string64 buff;
            if (item->m_text1)
            {
                xr_sprintf(buff, "%d.", place);
                item->m_text1->SetText(buff);
            }
            if (item->m_text2)
                item->m_text2->SetText(trader->m_character_name.c_str());
            if (item->m_text3)
            {
                xr_sprintf(buff, "%d", trader->Rank());
                item->m_text3->SetText(buff);
            }
        }
    }

    m_list->AddWindow(item, true);
}

void CUISHoCRankingWnd::SelectItem(CUIWindow* item)
{
    if (item)
        m_list->SetSelected(item);
}

void CUISHoCRankingWnd::ShowHumanInfo(u16 id)
{
    if (m_character_info && id != u16(-1))
        m_character_info->InitCharacter(id);
}
