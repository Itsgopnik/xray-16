#include "pch_script.h"
#include "UISHoCPdaAux.h"
#include "xrUICore/TabControl/UITabControl.h"
#include "xrUICore/TabControl/UITabButton.h"
#include "xrUICore/Static/UIStatic.h"
#include "UIXmlInit.h"

void shoc_init_auto_statics(CUIXml& xml, pcstr node, CUIWindow* parent)
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

void shoc_rearrange_tab_buttons(CUITabControl* tab, xr_vector<Fvector2>& sign_places)
{
    TABS_VECTOR& buttons = *tab->GetButtonsVector();
    sign_places.clear();
    sign_places.resize(buttons.size());
    if (buttons.empty())
        return;

    constexpr Fvector2 sign_size{ 9.0f + 3.0f, 11.0f };
    Fvector2 pos = buttons.front()->GetWndPos();
    for (u32 idx = 0; idx < buttons.size(); ++idx)
    {
        CUITabButton* button = buttons[idx];
        if (idx != 0)
        {
            CUIStatic* separator = xr_new<CUIStatic>("tab_separator");
            separator->SetAutoDelete(true);
            tab->AttachChild(separator);
            separator->SetFont(button->GetFont());
            separator->SetTextColor(color_rgba(90, 90, 90, 255));
            separator->SetText("//");
            separator->SetWndSize(button->GetWndSize());
            separator->AdjustWidthToText();
            separator->SetWndPos(pos);
            pos.x += separator->GetWndSize().x;
        }

        sign_places[idx].set(pos.x, std::floor(pos.y + (button->GetWndSize().y - sign_size.y) / 2.0f));
        pos.x += sign_size.x;

        button->SetWndPos(pos);
        button->AdjustWidthToText();
        pos.x += button->GetWndSize().x + 3.0f;
    }
}

void shoc_draw_tab_sign(CUITabControl* tab, const Fvector2& place, bool updated, CUIStatic* updated_sign, CUIStatic* old_sign)
{
    CUIStatic* sign = updated ? updated_sign : old_sign;
    if (!sign)
        return;
    Fvector2 pos;
    tab->GetAbsolutePos(pos);
    pos.add(place);
    sign->SetWndPos(pos);
    sign->Draw();
}
