#pragma once
// Shared helpers of the SoC PDA windows (from the original UIPdaAux / CUIPdaWnd).

class CUITabControl;
class CUIStatic;

// Bits in g_pda_info_state: PDA sections with unseen content ("updated" sign in the tab bar)
namespace pda_section
{
enum part : u32
{
    quests = (1 << 8),
    map = (1 << 9),
    diary = (1 << 10),
    contacts = (1 << 11),
    ranking = (1 << 12),
    statistics = (1 << 13),
    encyclopedia = (1 << 14),

    news = diary | (1 << 1),
    info = diary | (1 << 2),
    journal = diary | (1 << 3),
};
} // namespace pda_section

extern u32 g_pda_info_state;

// Packs the tab buttons as in SoC: "[sign] Tab1 // [sign] Tab2 ...", returns the sign positions
void shoc_rearrange_tab_buttons(CUITabControl* tab, xr_vector<Fvector2>& sign_places);
// Draws the "updated" or "not updated" sign at a position relative to the tab control
void shoc_draw_tab_sign(CUITabControl* tab, const Fvector2& place, bool updated, CUIStatic* updated_sign, CUIStatic* old_sign);
