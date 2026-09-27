#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/uiabstract.h"

class CUIScrollView;
class CUICharacterInfo;
class CUISHoCContactsWnd;

// Character card of one PDA contact (pda_character.xml)
class CUISHoCContactItem : public CUIWindow, public CUISelectable
{
    CUISHoCContactsWnd* m_owner;
    CUICharacterInfo* m_info{};

public:
    CUISHoCContactItem(CUISHoCContactsWnd* owner);

    void Init(float width, u16 owner_id);
    u16 OwnerID() const { return m_owner_id; }

    void SetSelected(bool b) override;
    bool OnMouseDown(int mouse_btn) override;

    pcstr GetDebugType() override { return "CUISHoCContactItem"; }

private:
    u16 m_owner_id{ u16(-1) };
};

// SOC "Contacts" PDA tab (original: CUIPdaContactsWnd)
// Lists the characters whose PDA is currently in range of the actor's PDA.
class CUISHoCContactsWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    CUIScrollView* m_list{};
    CUIScrollView* m_detail_list{};
    xr_vector<u16> m_contact_ids;

    void Rebuild(const xr_vector<u16>& ids);

public:
    CUISHoCContactsWnd();

    bool Init();
    void Show(bool status) override;
    void Update() override;

    void SelectItem(CUIWindow* item);
    void ShowDetails(u16 owner_id);

    pcstr GetDebugType() override { return "CUISHoCContactsWnd"; }
};
