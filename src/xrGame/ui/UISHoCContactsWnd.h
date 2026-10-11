#pragma once
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/uiabstract.h"

class CUIFrameWindow;
class CUIFrameLineWnd;
class CUIAnimatedStatic;
class CUIScrollView;
class CUICharacterInfo;
class CUIXml;

// SoC PDA contacts tab. Layout from the original pda_contacts_new.xml, one entry per
// active PDA contact laid out by pda_character.xml (ported from the SoC/OGSR
// CUIPdaContactsWnd).
class CUISHoCContactsWnd final : public CUIWindow
{
    using inherited = CUIWindow;

    CUIFrameWindow* m_left_frame{};
    CUIFrameLineWnd* m_left_header{};
    CUIFrameWindow* m_right_frame{};
    CUIFrameLineWnd* m_right_header{};
    CUIAnimatedStatic* m_animation{};
    CUIScrollView* m_list{};
    CUIScrollView* m_details{};

    void FillList();

public:
    CUISHoCContactsWnd();
    ~CUISHoCContactsWnd() override;

    bool Init();
    void Show(bool status) override;
    void ShowDetails(u16 owner_id);

    pcstr GetDebugType() override { return "CUISHoCContactsWnd"; }
};

class CUISHoCContactItem final : public CUIWindow, public CUISelectable
{
    using inherited = CUIWindow;

    CUISHoCContactsWnd* m_owner;
    CUICharacterInfo* m_info{};

public:
    CUISHoCContactItem(CUISHoCContactsWnd* owner);

    void Init(float width, float height, u16 character_id);
    void SetSelected(bool b) override;
    bool OnMouseDown(int mouse_btn) override;

    pcstr GetDebugType() override { return "CUISHoCContactItem"; }
};
