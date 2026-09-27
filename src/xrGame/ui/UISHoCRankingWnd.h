#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/uiabstract.h"

class CUIXml;
class CUIStatic;
class CUIScrollView;
class CUICharacterInfo;
class CUISHoCRankingWnd;

// One row of the SOC top list (item_human / item_actor / item_ellipsis / no_items in stalkers_ranking.xml)
class CUISHoCRankingItem : public CUIWindow, public CUISelectable
{
    CUISHoCRankingWnd* m_owner;
    u32 m_stored_alpha{ 255 };
    bool m_selectable;

public:
    CUIStatic* m_text1{};
    CUIStatic* m_text2{};
    CUIStatic* m_text3{};
    u16 m_humanID{ u16(-1) };

    CUISHoCRankingItem(CUISHoCRankingWnd* owner, bool selectable);

    void Init(CUIXml& xml, pcstr path);
    void SetSelected(bool b) override;
    bool OnMouseDown(int mouse_btn) override;

    pcstr GetDebugType() override { return "CUISHoCRankingItem"; }
};

// SOC "Top 20" PDA tab (original: CUIStalkersRankingWnd)
class CUISHoCRankingWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    CUIScrollView* m_list{};
    CUICharacterInfo* m_character_info{};

    void FillList();
    void AddItem(CUIXml& xml, pcstr path, int place, u16 id, bool selectable);

public:
    CUISHoCRankingWnd();

    bool Init();
    void Show(bool status) override;
    void ShowHumanInfo(u16 id);
    void SelectItem(CUIWindow* item);

    pcstr GetDebugType() override { return "CUISHoCRankingWnd"; }
};
