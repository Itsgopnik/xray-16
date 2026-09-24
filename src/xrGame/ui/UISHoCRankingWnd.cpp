#include "pch_script.h"
#include "UISHoCRankingWnd.h"

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
#include "xrServerEntities/xrServer_Objects_ALife_Monsters.h"

#define SHOC_RANKING_XML "actor_diary.xml"

namespace
{
    constexpr u32 kColorTitle = color_argb(255, 231, 168, 12);
    constexpr u32 kColorRow = color_argb(255, 225, 225, 250);
    constexpr u32 kColorMuted = color_argb(255, 181, 170, 148);

    constexpr float kColIdxW = 32.0f;
    constexpr float kColNameW = 180.0f;
    constexpr float kColRankW = 120.0f;
    constexpr float kColCommunityW = 86.0f;
}

CUISHoCRankingWnd::CUISHoCRankingWnd() : CUIWindow("CUISHoCRankingWnd") {}
CUISHoCRankingWnd::~CUISHoCRankingWnd() = default;

bool CUISHoCRankingWnd::Init()
{
    SetWndPos({ 110.0f, 90.0f });
    SetWndSize({ 761.0f, 475.0f });

    CUIXml uiXml;
    const bool xmlLoaded = uiXml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_RANKING_XML, false);

    m_title = xr_new<CUIStatic>("RankingTitle");
    m_title->SetAutoDelete(true);
    AttachChild(m_title);
    m_title->SetWndPos({ 21.0f, 56.0f });
    m_title->SetWndSize({ 420.0f, 22.0f });
    m_title->TextItemControl()->SetTextColor(kColorTitle);
    m_title->TextItemControl()->SetTextST("st_ui_pda_top20_title");

    m_header = xr_new<CUIStatic>("RankingHeader");
    m_header->SetAutoDelete(true);
    AttachChild(m_header);
    m_header->SetWndPos({ 21.0f, 72.0f });
    m_header->SetWndSize({ 420.0f, 14.0f });
    m_header->TextItemControl()->SetTextColor(kColorMuted);
    m_header->TextItemControl()->SetText("#    Name                      Rank             Faction");

    m_list = xr_new<CUIListBox>();
    m_list->SetAutoDelete(true);
    AttachChild(m_list);
    if (!(xmlLoaded && CUIXmlInit::InitListBox(uiXml, "list", 0, m_list, false)))
    {
        m_list->SetWndPos({ 21.0f, 88.0f });
        m_list->SetWndSize({ 420.0f, 375.0f });
        m_list->SetItemHeight(16.0f);
    }
    m_list->SetTextColor(kColorRow);

    m_empty_label = xr_new<CUIStatic>("RankingEmpty");
    m_empty_label->SetAutoDelete(true);
    AttachChild(m_empty_label);
    m_empty_label->SetWndPos({ 21.0f, 200.0f });
    m_empty_label->SetWndSize({ 420.0f, 22.0f });
    m_empty_label->TextItemControl()->SetTextColor(kColorMuted);
    m_empty_label->TextItemControl()->SetTextST("st_ui_pda_top20_empty");
    m_empty_label->Show(false);

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
    if (!m_list)
        return;

    m_list->Clear();

    if (!ai().get_alife())
    {
        m_empty_label->Show(true);
        return;
    }

    struct Entry
    {
        CHARACTER_RANK_VALUE rank;
        xr_string name;
        shared_str community;
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

        Entry e;
        e.rank = stalker->CSE_ALifeTraderAbstract::Rank();
        e.name = stalker->CSE_ALifeTraderAbstract::m_character_name;
        e.community = CHARACTER_COMMUNITY::IndexToId(
            stalker->CSE_ALifeTraderAbstract::m_community_index);
        entries.push_back(e);
    }

    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.rank > b.rank; });

    const size_t count = entries.size() < 20 ? entries.size() : 20;
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
        // CUIListBox::AddItem stretches the default text field to full list width;
        // collapse it so AddTextField() starts at x=0 instead of x=full_width.
        item->GetTextItem()->SetWidth(0.0f);

        string32 idx_buf;
        xr_sprintf(idx_buf, sizeof(idx_buf), "%2u.", static_cast<u32>(i + 1));
        item->AddTextField(idx_buf, kColIdxW);

        LPCSTR name = entries[i].name.empty() ? "<unnamed>" : entries[i].name.c_str();
        item->AddTextField(name, kColNameW);

        LPCSTR rank_key = InventoryUtilities::GetRankAsText(entries[i].rank);
        shared_str rank_translated = StringTable().translate(rank_key);
        item->AddTextField(rank_translated.c_str(), kColRankW);

        shared_str comm_translated = StringTable().translate(entries[i].community);
        item->AddTextField(comm_translated.c_str(), kColCommunityW);
    }
}
