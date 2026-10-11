#include "pch_script.h"
#include "UISHoCContactsWnd.h"

#include "UIXmlInit.h"
#include "UICharacterInfo.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrUICore/Static/UIAnimatedStatic.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrUICore/ScrollView/UIScrollView.h"
#include "Actor.h"
#include "PDA.h"
#include "character_info.h"
#include "xrEngine/StringTable/StringTable.h"

#define SHOC_CONTACTS_XML "pda_contacts_new.xml"
#define SHOC_CONTACT_CHARACTER_XML "pda_character.xml"

namespace
{
constexpr float kContactHeight = 85.0f;

// The original layouts repeat a node (e.g. left_auto_static) once per static
void init_auto_statics(CUIXml& xml, pcstr node, CUIWindow* parent)
{
    const int count = xml.GetNodesNum(xml.GetRoot(), node);
    for (int i = 0; i < count; ++i)
    {
        CUIStatic* st = xr_new<CUIStatic>(node);
        st->SetAutoDelete(true);
        CUIXmlInit::InitStatic(xml, node, i, st);
        parent->AttachChild(st);
    }
}
} // namespace

CSE_ALifeTraderAbstract* ch_info_get_from_id(u16 id);

CUISHoCContactsWnd::CUISHoCContactsWnd() : CUIWindow("CUISHoCContactsWnd") {}
CUISHoCContactsWnd::~CUISHoCContactsWnd() = default;

bool CUISHoCContactsWnd::Init()
{
    CUIXml xml;
    if (!xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_CONTACTS_XML, false))
        return false;

    CUIXmlInit::InitWindow(xml, "main_wnd", 0, this);

    m_left_frame = xr_new<CUIFrameWindow>("left_frame_window");
    m_left_frame->SetAutoDelete(true);
    AttachChild(m_left_frame);
    CUIXmlInit::InitFrameWindow(xml, "left_frame_window", 0, m_left_frame);

    m_left_header = xr_new<CUIFrameLineWnd>("left_frame_line");
    m_left_header->SetAutoDelete(true);
    m_left_frame->AttachChild(m_left_header);
    CUIXmlInit::InitFrameLine(xml, "left_frame_line", 0, m_left_header);

    m_right_frame = xr_new<CUIFrameWindow>("right_frame_window");
    m_right_frame->SetAutoDelete(true);
    AttachChild(m_right_frame);
    CUIXmlInit::InitFrameWindow(xml, "right_frame_window", 0, m_right_frame);

    m_right_header = xr_new<CUIFrameLineWnd>("right_frame_line");
    m_right_header->SetAutoDelete(true);
    m_right_frame->AttachChild(m_right_header);
    CUIXmlInit::InitFrameLine(xml, "right_frame_line", 0, m_right_header);

    if (xml.NavigateToNode("a_static", 0))
    {
        m_animation = xr_new<CUIAnimatedStatic>();
        m_animation->SetAutoDelete(true);
        m_left_header->AttachChild(m_animation);
        CUIXmlInit::InitAnimatedStatic(xml, "a_static", 0, m_animation);
    }

    m_list = xr_new<CUIScrollView>();
    m_list->SetAutoDelete(true);
    m_left_frame->AttachChild(m_list);
    CUIXmlInit::InitScrollView(xml, "list", 0, m_list);

    m_details = xr_new<CUIScrollView>();
    m_details->SetAutoDelete(true);
    m_right_frame->AttachChild(m_details);
    CUIXmlInit::InitScrollView(xml, "detail_list", 0, m_details);

    init_auto_statics(xml, "left_auto_static", m_left_frame);
    init_auto_statics(xml, "right_auto_static", m_right_frame);
    return true;
}

void CUISHoCContactsWnd::Show(bool status)
{
    inherited::Show(status);
    if (status)
        FillList();
}

void CUISHoCContactsWnd::FillList()
{
    m_list->Clear();
    m_details->Clear();

    CActor* actor = Actor();
    CPda* pda = actor ? actor->GetPDA() : nullptr;
    if (!pda)
        return;

    xr_vector<CPda*> contacts;
    pda->ActivePDAContacts(contacts);
    for (CPda* contact : contacts)
    {
        CInventoryOwner* owner = contact->GetOriginalOwner();
        if (!owner)
            continue;
        auto* item = xr_new<CUISHoCContactItem>(this);
        item->Init(m_list->GetDesiredChildWidth(), kContactHeight, owner->object_id());
        m_list->AddWindow(item, true);
    }
}

void CUISHoCContactsWnd::ShowDetails(u16 owner_id)
{
    m_details->Clear();

    CSE_ALifeTraderAbstract* trader = ch_info_get_from_id(owner_id);
    if (!trader)
        return;
    CCharacterInfo info;
    info.Init(trader);
    if (info.Bio().empty())
        return;

    CUIStatic* bio = xr_new<CUIStatic>("ContactBio");
    bio->SetTextComplexMode(true);
    bio->SetWidth(m_details->GetDesiredChildWidth());
    bio->TextItemControl()->SetText(StringTable().translate(info.Bio()).c_str());
    bio->AdjustHeightToText();
    m_details->AddWindow(bio, true);
}

CUISHoCContactItem::CUISHoCContactItem(CUISHoCContactsWnd* owner)
    : CUIWindow("CUISHoCContactItem"), m_owner(owner) {}

void CUISHoCContactItem::Init(float width, float height, u16 character_id)
{
    SetWndSize({ width, height });

    CUIXml xml;
    xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, SHOC_CONTACT_CHARACTER_XML);

    m_info = xr_new<CUICharacterInfo>();
    m_info->SetAutoDelete(true);
    AttachChild(m_info);
    m_info->InitCharacterInfo(Fvector2().set(0, 0), Fvector2().set(width, height), &xml);
    m_info->InitCharacter(character_id);

    CUIXmlInit::InitAutoStaticGroup(xml, "pda_char_auto_statics", 0, this);
}

void CUISHoCContactItem::SetSelected(bool b)
{
    CUISelectable::SetSelected(b);
    if (b)
        m_owner->ShowDetails(m_info->OwnerID());
}

bool CUISHoCContactItem::OnMouseDown(int mouse_btn)
{
    if (mouse_btn == MOUSE_1)
    {
        // Items live on the scroll view's pad: parent is the pad, its parent the view
        if (CUIScrollView* list = smart_cast<CUIScrollView*>(GetParent() ? GetParent()->GetParent() : nullptr))
            list->SetSelected(this);
        if (!GetSelected())
            SetSelected(true);
        return true;
    }
    return inherited::OnMouseDown(mouse_btn);
}
