#include "pch_script.h"
#include "UISHoCContactsWnd.h"

#include "UIXmlInit.h"
#include "UIHelper.h"
#include "UICharacterInfo.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "Actor.h"
#include "PDA.h"
#include "xrServerEntities/character_info.h"

#define SHOC_CONTACTS_XML "pda_contacts_new.xml"
#define SHOC_CONTACT_CHARACTER_XML "pda_character.xml"

CSE_ALifeTraderAbstract* ch_info_get_from_id(u16 id);

namespace
{
constexpr float CONTACT_ITEM_HEIGHT = 85.0f;
}

CUISHoCContactItem::CUISHoCContactItem(CUISHoCContactsWnd* owner)
    : CUIWindow("CUISHoCContactItem"), m_owner(owner) {}

void CUISHoCContactItem::Init(float width, u16 owner_id)
{
    m_owner_id = owner_id;
    SetWndSize({ width, CONTACT_ITEM_HEIGHT });

    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_CONTACT_CHARACTER_XML, false))
        return;

    m_info = xr_new<CUICharacterInfo>();
    m_info->SetAutoDelete(true);
    AttachChild(m_info);
    m_info->InitCharacterInfo({ 0.0f, 0.0f }, GetWndSize(), &xml);
    m_info->InitCharacter(owner_id);

    CUIXmlInit::InitAutoStaticGroup(xml, "pda_char_auto_statics", 0, this);
}

void CUISHoCContactItem::SetSelected(bool b)
{
    CUISelectable::SetSelected(b);
    if (b)
        m_owner->ShowDetails(m_owner_id);
}

bool CUISHoCContactItem::OnMouseDown(int mouse_btn)
{
    if (mouse_btn == MOUSE_1)
    {
        m_owner->SelectItem(this);
        return true;
    }
    return false;
}

CUISHoCContactsWnd::CUISHoCContactsWnd() : CUIWindow("CUISHoCContactsWnd") {}

bool CUISHoCContactsWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_CONTACTS_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    // Left part: contacts
    CUIWindow* left = this;
    CUIWindow* left_header = nullptr;
    if (CUIFrameWindow* frame = UIHelper::CreateFrameWindow(xml, "left_frame_window", this, false))
    {
        left = frame;
        left_header = UIHelper::CreateFrameLine(xml, "left_frame_line", frame, false);
    }

    // Right part: details of the selected contact
    CUIWindow* right = this;
    if (CUIFrameWindow* frame = UIHelper::CreateFrameWindow(xml, "right_frame_window", this, false))
    {
        right = frame;
        UIHelper::CreateFrameLine(xml, "right_frame_line", frame, false);
    }

    if (xml.NavigateToNode("a_static"))
    {
        auto* anim = xr_new<CUIAnimatedStatic>();
        anim->SetAutoDelete(true);
        (left_header ? left_header : left)->AttachChild(anim);
        CUIXmlInit::InitAnimatedStatic(xml, "a_static", 0, anim);
    }

    m_list = UIHelper::CreateScrollView(xml, "list", left, false);
    if (!m_list)
        return false;
    m_detail_list = UIHelper::CreateScrollView(xml, "detail_list", right, false);

    UIHelper::CreateStatics(xml, "left_auto_static", left);
    UIHelper::CreateStatics(xml, "right_auto_static", right);

    return true;
}

void CUISHoCContactsWnd::Show(bool status)
{
    if (status)
    {
        // Force a rebuild on the next Update()
        m_contact_ids.clear();
        m_list->Clear();
        if (m_detail_list)
            m_detail_list->Clear();
    }
    inherited::Show(status);
}

void CUISHoCContactsWnd::Update()
{
    xr_vector<u16> ids;
    if (CPda* pda = Actor() ? Actor()->GetPDA() : nullptr)
    {
        xr_vector<CPda*> contacts;
        pda->ActivePDAContacts(contacts);
        ids.reserve(contacts.size());
        for (CPda* contact : contacts)
            ids.push_back(contact->GetOriginalOwnerID());
    }

    if (ids != m_contact_ids)
        Rebuild(ids);

    inherited::Update();
}

void CUISHoCContactsWnd::Rebuild(const xr_vector<u16>& ids)
{
    u16 selected_id = u16(-1);
    if (auto* selected = smart_cast<CUISHoCContactItem*>(m_list->GetSelected()))
        selected_id = selected->OwnerID();

    m_contact_ids = ids;
    m_list->Clear();
    if (m_detail_list)
        m_detail_list->Clear();

    CUIWindow* to_select = nullptr;
    for (const u16 id : ids)
    {
        if (!ch_info_get_from_id(id))
            continue;

        auto* item = xr_new<CUISHoCContactItem>(this);
        item->Init(m_list->GetDesiredChildWidth(), id);
        m_list->AddWindow(item, true);

        if (id == selected_id)
            to_select = item;
    }

    SelectItem(to_select);
}

void CUISHoCContactsWnd::SelectItem(CUIWindow* item)
{
    if (item)
        m_list->SetSelected(item);
}

void CUISHoCContactsWnd::ShowDetails(u16 owner_id)
{
    if (!m_detail_list)
        return;

    m_detail_list->Clear();

    CSE_ALifeTraderAbstract* trader = ch_info_get_from_id(owner_id);
    if (!trader)
        return;

    CCharacterInfo info;
    info.Init(trader);
    if (info.Bio().empty())
        return;

    auto* bio = xr_new<CUIStatic>("Biography");
    bio->SetWidth(m_detail_list->GetDesiredChildWidth());
    bio->SetTextComplexMode(true);
    bio->SetText(info.Bio().c_str());
    bio->AdjustHeightToText();
    m_detail_list->AddWindow(bio, true);
}
