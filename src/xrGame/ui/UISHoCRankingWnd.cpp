#include "pch_script.h"
#include "UISHoCRankingWnd.h"

#include "UIXmlInit.h"
#include "UICharacterInfo.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "Actor.h"
#include "xrServerEntities/xrServer_Objects_ALife_Monsters.h"

#define SHOC_RANKING_XML "stalkers_ranking.xml"
#define SHOC_RANKING_CHARACTER_XML "stalkers_ranking_character.xml"

CSE_ALifeTraderAbstract* ch_info_get_from_id(u16 id);

namespace
{
constexpr int kTopCount = 20;
xr_vector<u16> g_ranking;

bool greater_rank(u16 a, u16 b)
{
    CSE_ALifeTraderAbstract* ta = ch_info_get_from_id(a);
    CSE_ALifeTraderAbstract* tb = ch_info_get_from_id(b);
    if (ta && tb)
        return ta->m_rank > tb->m_rank;
    return ta != nullptr;
}
} // namespace

void shoc_ranking_remove(u16 id) { g_ranking.erase(std::remove(g_ranking.begin(), g_ranking.end(), id), g_ranking.end()); }

void shoc_ranking_add(u16 id)
{
    if (!ch_info_get_from_id(id))
    {
        Msg("! [shoc ranking] id[%d] is not a CSE_ALifeTraderAbstract", id);
        return;
    }
    shoc_ranking_remove(id);
    g_ranking.push_back(id);
}

void shoc_ranking_clear() { g_ranking.clear(); }

CUISHoCRankingWnd::CUISHoCRankingWnd() : CUIWindow("CUISHoCRankingWnd") {}
CUISHoCRankingWnd::~CUISHoCRankingWnd() = default;

bool CUISHoCRankingWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_RANKING_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    m_icon_frame = xr_new<CUIFrameWindow>("chicon_frame_window");
    m_icon_frame->SetAutoDelete(true);
    AttachChild(m_icon_frame);
    CUIXmlInit::InitFrameWindow(xml, "chicon_frame_window", 0, m_icon_frame);

    m_icon_header = xr_new<CUIFrameLineWnd>("chicon_frame_line");
    m_icon_header->SetAutoDelete(true);
    m_icon_frame->AttachChild(m_icon_header);
    CUIXmlInit::InitFrameLine(xml, "chicon_frame_line", 0, m_icon_header);

    m_info_frame = xr_new<CUIFrameWindow>("info_frame_window");
    m_info_frame->SetAutoDelete(true);
    AttachChild(m_info_frame);
    CUIXmlInit::InitFrameWindow(xml, "info_frame_window", 0, m_info_frame);

    m_info_header = xr_new<CUIFrameLineWnd>("info_frame_line");
    m_info_header->SetAutoDelete(true);
    m_info_frame->AttachChild(m_info_header);
    CUIXmlInit::InitFrameLine(xml, "info_frame_line", 0, m_info_header);

    if (xml.NavigateToNode("a_static", 0))
    {
        m_animation = xr_new<CUIAnimatedStatic>();
        m_animation->SetAutoDelete(true);
        m_info_header->AttachChild(m_animation);
        CUIXmlInit::InitAnimatedStatic(xml, "a_static", 0, m_animation);
    }

    m_list = xr_new<CUIScrollView>();
    m_list->SetAutoDelete(true);
    m_info_frame->AttachChild(m_list);
    CUIXmlInit::InitScrollView(xml, "list", 0, m_list);

    CUIWindow* character_window = xr_new<CUIWindow>("character_info");
    character_window->SetAutoDelete(true);
    m_icon_frame->AttachChild(character_window);
    CUIXmlInit::InitWindow(xml, "character_info", 0, character_window);

    CUIXml character_xml;
    character_xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_RANKING_CHARACTER_XML);
    m_character_info = xr_new<CUICharacterInfo>();
    m_character_info->SetAutoDelete(true);
    character_window->AttachChild(m_character_info);
    m_character_info->InitCharacterInfo(Fvector2().set(0, 0), character_window->GetWndSize(), &character_xml);

    CUIXmlInit::InitAutoStaticGroup(xml, "left_auto", 0, m_info_frame);
    CUIXmlInit::InitAutoStaticGroup(xml, "right_auto", 0, m_icon_frame);
    return true;
}

void CUISHoCRankingWnd::Show(bool status)
{
    inherited::Show(status);
    if (status)
        FillList();
}

void CUISHoCRankingWnd::FillList()
{
    m_list->Clear();

    CUIXml xml;
    xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_RANKING_XML);
    xml.SetLocalRoot(xml.NavigateToNode("stalkers_list", 0));

    if (g_ranking.empty())
    {
        auto* item = xr_new<CUISHoCRankingItem>(this);
        item->Init(xml, "no_items", false);
        m_list->AddWindow(item, true);
        return;
    }

    std::sort(g_ranking.begin(), g_ranking.end(), greater_rank);

    CActor* actor = Actor();
    CSE_ALifeTraderAbstract* actor_trader = actor ? ch_info_get_from_id(actor->ID()) : nullptr;
    const auto actor_it = actor ? std::find(g_ranking.begin(), g_ranking.end(), actor->ID()) : g_ranking.end();
    const int actor_place = actor_it != g_ranking.end() ? int(actor_it - g_ranking.begin()) : -1;

    const int count = std::min<int>(kTopCount, int(g_ranking.size()));
    for (int i = 0; i < count; ++i)
    {
        CSE_ALifeTraderAbstract* trader = ch_info_get_from_id(g_ranking[i]);
        if (!trader)
            continue;
        // The actor is always listed: on its place, or in the last line after an ellipsis
        if (actor_trader && (trader == actor_trader || (i == kTopCount - 1 && actor_place >= kTopCount)))
        {
            if (actor_place >= kTopCount)
            {
                auto* ellipsis = xr_new<CUISHoCRankingItem>(this);
                ellipsis->Init(xml, "item_ellipsis", false);
                m_list->AddWindow(ellipsis, true);
            }
            AddItem(xml, "item_actor", actor_place + 1, actor_trader);
        }
        else
            AddItem(xml, "item_human", i + 1, trader);
    }

    m_list->SetSelected(m_list->GetItem(0));
}

void CUISHoCRankingWnd::AddItem(CUIXml& xml, pcstr node, int place, CSE_ALifeTraderAbstract* trader)
{
    auto* item = xr_new<CUISHoCRankingItem>(this);
    item->Init(xml, node, true);

    string32 buf;
    xr_sprintf(buf, "%d.", place);
    item->m_text[0]->TextItemControl()->SetText(buf);
    item->m_text[1]->TextItemControl()->SetText(trader->m_character_name.c_str());
    xr_sprintf(buf, "%d", trader->m_rank);
    item->m_text[2]->TextItemControl()->SetText(buf);
    item->m_human_id = trader->object_id();
    m_list->AddWindow(item, true);
}

void CUISHoCRankingWnd::ShowHumanInfo(u16 id) { m_character_info->InitCharacter(id); }

CUISHoCRankingItem::CUISHoCRankingItem(CUISHoCRankingWnd* owner)
    : CUIWindow("CUISHoCRankingItem"), m_owner(owner) {}

void CUISHoCRankingItem::Init(CUIXml& xml, pcstr node, bool selectable)
{
    m_selectable = selectable;
    XML_NODE stored_root = xml.GetLocalRoot();

    CUIXmlInit::InitWindow(xml, node, 0, this);
    xml.SetLocalRoot(xml.NavigateToNode(node, 0));

    constexpr pcstr text_nodes[] = { "text_1", "text_2", "text_3" };
    for (int i = 0; i < 3; ++i)
    {
        m_text[i] = xr_new<CUIStatic>(text_nodes[i]);
        m_text[i]->SetAutoDelete(true);
        AttachChild(m_text[i]);
        CUIXmlInit::InitStatic(xml, text_nodes[i], 0, m_text[i], false);
    }
    CUIXmlInit::InitAutoStaticGroup(xml, "auto", 0, this);

    m_stored_alpha = color_get_A(m_text[1]->GetTextColor());
    xml.SetLocalRoot(stored_root);
}

void CUISHoCRankingItem::SetSelected(bool b)
{
    if (!m_selectable)
        return;
    CUISelectable::SetSelected(b);
    for (CUIStatic* text : m_text)
        text->SetTextColor(subst_alpha(text->GetTextColor(), b ? 255 : m_stored_alpha));
    if (b && m_human_id != u16(-1))
        m_owner->ShowHumanInfo(m_human_id);
}

bool CUISHoCRankingItem::OnMouseDown(int mouse_btn)
{
    if (mouse_btn == MOUSE_1 && m_selectable)
    {
        m_owner->GetTopList().SetSelected(this);
        return true;
    }
    return inherited::OnMouseDown(mouse_btn);
}
