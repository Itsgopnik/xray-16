#include "pch_script.h"
#include "UISHoCContactsWnd.h"

#include "UIXmlInit.h"
#include "UIInventoryUtilities.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/ListBox/UIListBox.h"
#include "xrUICore/ListBox/UIListBoxItem.h"
#include "alife_simulator.h"
#include "alife_object_registry.h"
#include "ai_space.h"
#include "xrAICore/Navigation/game_graph.h"
#include "character_community.h"
#include "relation_registry.h"
#include "Actor.h"
#include "xrServerEntities/xrServer_Objects_ALife_Monsters.h"

#define SHOC_CONTACTS_XML "pda_contacts_new.xml"

namespace
{
    constexpr u32 kColorTitle = color_argb(255, 231, 168, 12);
    constexpr u32 kColorRow = color_argb(255, 225, 225, 250);
    constexpr u32 kColorMuted = color_argb(255, 181, 170, 148);

    constexpr float kColNameW = 190.0f;
    constexpr float kColFactionW = 120.0f;
}

CUISHoCContactsWnd::CUISHoCContactsWnd() : CUIWindow("CUISHoCContactsWnd") {}
CUISHoCContactsWnd::~CUISHoCContactsWnd() = default;

bool CUISHoCContactsWnd::Init()
{
    SetWndPos({ 110.0f, 90.0f });
    SetWndSize({ 761.0f, 475.0f });

    CUIXml uiXml;
    uiXml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_CONTACTS_XML, false);

    m_title = xr_new<CUIStatic>("ContactsTitle");
    m_title->SetAutoDelete(true);
    AttachChild(m_title);
    m_title->SetWndPos({ 21.0f, 56.0f });
    m_title->SetWndSize({ 340.0f, 22.0f });
    m_title->TextItemControl()->SetTextColor(kColorTitle);
    m_title->TextItemControl()->SetTextST("st_ui_pda_contacts_title");

    m_list = xr_new<CUIListBox>();
    m_list->SetAutoDelete(true);
    AttachChild(m_list);
    m_list->SetWndPos({ 21.0f, 82.0f });
    m_list->SetWndSize({ 320.0f, 381.0f });
    m_list->SetItemHeight(16.0f);
    m_list->SetTextColor(kColorRow);
    m_list->SetMessageTarget(this);

    Register(m_list);
    AddCallback(m_list, LIST_ITEM_CLICKED,
        CUIWndCallback::void_function(this, &CUISHoCContactsWnd::OnContactSelected));
    AddCallback(m_list, LIST_ITEM_SELECT,
        CUIWndCallback::void_function(this, &CUISHoCContactsWnd::OnContactSelected));

    // Right pane: selected contact detail
    auto make_detail = [&](pcstr name, float y, u32 color) -> CUIStatic*
    {
        CUIStatic* s = xr_new<CUIStatic>(name);
        s->SetAutoDelete(true);
        AttachChild(s);
        s->SetWndPos({ 360.0f, y });
        s->SetWndSize({ 380.0f, 22.0f });
        s->TextItemControl()->SetTextColor(color);
        s->TextItemControl()->SetText("");
        return s;
    };
    m_detail_name = make_detail("ContactName", 82.0f, kColorTitle);
    m_detail_faction = make_detail("ContactFaction", 112.0f, kColorRow);
    m_detail_rank = make_detail("ContactRank", 138.0f, kColorRow);
    m_detail_attitude = make_detail("ContactAttitude", 164.0f, kColorRow);

    m_empty_label = xr_new<CUIStatic>("ContactsEmpty");
    m_empty_label->SetAutoDelete(true);
    AttachChild(m_empty_label);
    m_empty_label->SetWndPos({ 21.0f, 200.0f });
    m_empty_label->SetWndSize({ 320.0f, 22.0f });
    m_empty_label->TextItemControl()->SetTextColor(kColorMuted);
    m_empty_label->TextItemControl()->SetTextST("st_ui_pda_contacts_empty");
    m_empty_label->Show(false);

    return true;
}

void CUISHoCContactsWnd::Show(bool status)
{
    if (status)
        FillList();
    inherited::Show(status);
}

void CUISHoCContactsWnd::FillList()
{
    if (!m_list)
        return;

    m_list->Clear();
    m_detail_name->TextItemControl()->SetText("");
    m_detail_faction->TextItemControl()->SetText("");
    m_detail_rank->TextItemControl()->SetText("");
    m_detail_attitude->TextItemControl()->SetText("");

    if (!ai().get_alife())
    {
        m_empty_label->Show(true);
        return;
    }

    struct Entry
    {
        u16 id;
        CHARACTER_RANK_VALUE rank;
        xr_string name;
    };
    xr_vector<Entry> entries;
    entries.reserve(128);

    const auto& objects = ai().alife().objects().objects();
    for (auto it = objects.begin(); it != objects.end(); ++it)
    {
        CSE_ALifeHumanStalker* stalker = smart_cast<CSE_ALifeHumanStalker*>(it->second);
        if (!stalker)
            continue;
        if (!stalker->g_Alive())
            continue;
        // Skip the actor itself and unnamed generic spawns.
        if (stalker->CSE_ALifeTraderAbstract::m_character_name.empty())
            continue;

        Entry e;
        e.id = stalker->ID;
        e.rank = stalker->CSE_ALifeTraderAbstract::Rank();
        e.name = stalker->CSE_ALifeTraderAbstract::m_character_name;
        entries.push_back(e);
    }

    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.rank > b.rank; });

    const size_t count = entries.size() < 50 ? entries.size() : 50;
    if (count == 0)
    {
        m_empty_label->Show(true);
        return;
    }
    m_empty_label->Show(false);

    for (size_t i = 0; i < count; ++i)
    {
        CUIListBoxItem* item = m_list->AddItem();
        item->SetTextColor(kColorRow);
        item->GetTextItem()->SetWidth(0.0f);
        item->SetTAG(entries[i].id);
        item->AddTextField(entries[i].name.c_str(), kColNameW);

        LPCSTR rank_key = InventoryUtilities::GetRankAsText(entries[i].rank);
        shared_str rank_translated = StringTable().translate(rank_key);
        item->AddTextField(rank_translated.c_str(), kColFactionW);
    }

    m_list->SetSelectedIDX(0);
    ShowSelectedDetail();
}

void CUISHoCContactsWnd::OnContactSelected(CUIWindow*, void*)
{
    ShowSelectedDetail();
}

void CUISHoCContactsWnd::ShowSelectedDetail()
{
    CUIListBoxItem* sel = m_list ? m_list->GetSelectedItem() : nullptr;
    if (!sel || !ai().get_alife())
        return;

    const u16 id = static_cast<u16>(sel->GetTAG());
    CSE_ALifeHumanStalker* stalker =
        smart_cast<CSE_ALifeHumanStalker*>(ai().alife().objects().object(id, true));
    if (!stalker)
        return;

    m_detail_name->TextItemControl()->SetText(
        stalker->CSE_ALifeTraderAbstract::m_character_name.c_str());

    shared_str community = CHARACTER_COMMUNITY::IndexToId(
        stalker->CSE_ALifeTraderAbstract::m_community_index);
    m_detail_faction->TextItemControl()->SetText(
        StringTable().translate(community).c_str());

    LPCSTR rank_key = InventoryUtilities::GetRankAsText(stalker->CSE_ALifeTraderAbstract::Rank());
    m_detail_rank->TextItemControl()->SetText(StringTable().translate(rank_key).c_str());

    // Faction goodwill toward the actor.
    if (Actor())
    {
        CHARACTER_GOODWILL gw = RELATION_REGISTRY().GetCommunityGoodwill(
            stalker->CSE_ALifeTraderAbstract::m_community_index, Actor()->ID());
        LPCSTR att_key = InventoryUtilities::GetGoodwillAsText(gw);
        m_detail_attitude->TextItemControl()->SetTextColor(
            InventoryUtilities::GetGoodwillColor(gw));
        m_detail_attitude->TextItemControl()->SetText(
            StringTable().translate(att_key).c_str());
    }
}
