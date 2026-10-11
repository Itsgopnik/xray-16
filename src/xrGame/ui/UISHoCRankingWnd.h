#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/uiabstract.h"

class CUIFrameWindow;
class CUIFrameLineWnd;
class CUIAnimatedStatic;
class CUIStatic;
class CUIScrollView;
class CUICharacterInfo;
class CUIXml;
class CSE_ALifeTraderAbstract;

// Ranking list of SoC: filled by the game scripts through actor_stats.add_to_ranking() /
// remove_from_ranking() (se_stalker, bind_stalker), as in the original engine.
void shoc_ranking_add(u16 id);
void shoc_ranking_remove(u16 id);
void shoc_ranking_clear();

// SoC PDA "Top 20 stalkers" tab, built from the original stalkers_ranking.xml
// (ported from the SoC/OGSR CUIStalkersRankingWnd).
class CUISHoCRankingWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    CUIFrameWindow* m_info_frame{};
    CUIFrameLineWnd* m_info_header{};
    CUIFrameWindow* m_icon_frame{};
    CUIFrameLineWnd* m_icon_header{};
    CUIAnimatedStatic* m_animation{};
    CUIScrollView* m_list{};
    CUICharacterInfo* m_character_info{};

    void FillList();
    void AddItem(CUIXml& xml, pcstr node, int place, CSE_ALifeTraderAbstract* trader);

public:
    CUISHoCRankingWnd();
    ~CUISHoCRankingWnd() override;

    bool Init();
    void Show(bool status) override;
    void ShowHumanInfo(u16 id);
    CUIScrollView& GetTopList() { return *m_list; }

    pcstr GetDebugType() override { return "CUISHoCRankingWnd"; }
};

class CUISHoCRankingItem final : public CUIWindow, public CUISelectable
{
    using inherited = CUIWindow;

    CUISHoCRankingWnd* m_owner;
    u32 m_stored_alpha{ 255 };
    bool m_selectable{ true };

public:
    u16 m_human_id{ u16(-1) };
    CUIStatic* m_text[3]{};

    CUISHoCRankingItem(CUISHoCRankingWnd* owner);

    void Init(CUIXml& xml, pcstr node, bool selectable);
    void SetSelected(bool b) override;
    bool OnMouseDown(int mouse_btn) override;

    pcstr GetDebugType() override { return "CUISHoCRankingItem"; }
};
