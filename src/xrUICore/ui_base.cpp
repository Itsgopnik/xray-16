#include "pch.hpp"
#include "ui_base.h"
#include "Cursor/UICursor.h"
#include "xrCore/XML/XMLDocument.hpp"
#include "XML/UIXmlInitBase.h"
#include "XML/UITextureMaster.h"

CUICursor& GetUICursor() { return GEnv.UI->GetUICursor(); }
UICore& UI() { return *GEnv.UI; }
extern ENGINE_API Fvector2 g_current_font_scale;
extern ENGINE_API bool ShadowOfChernobylMode;

void S2DVert::rotate_pt(const Fvector2& pivot, const float cosA, const float sinA, const float kx)
{
    Fvector2 t = pt;
    t.sub(pivot);
    pt.x = t.x * cosA + t.y * sinA;
    pt.y = t.y * cosA - t.x * sinA;
    pt.x *= kx;
    pt.add(pivot);
}
void C2DFrustum::CreateFromRect(const Frect& rect)
{
    m_rect.set(float(rect.x1), float(rect.y1), float(rect.x2), float(rect.y2));
    planes.resize(4);
    planes[0].build(rect.lt, Fvector2().set(-1, 0));
    planes[1].build(rect.lt, Fvector2().set(0, -1));
    planes[2].build(rect.rb, Fvector2().set(+1, 0));
    planes[3].build(rect.rb, Fvector2().set(0, +1));
}

sPoly2D* C2DFrustum::ClipPoly(sPoly2D& S, sPoly2D& D) const
{
    bool bFullTest = false;
    for (u32 j = 0; j < S.size(); j++)
    {
        if (!m_rect.in(S[j].pt))
        {
            bFullTest = true;
            break;
        }
    }

    sPoly2D* src = &D;
    sPoly2D* dest = &S;
    if (!bFullTest)
        return dest;

    for (u32 i = 0; i < planes.size(); i++)
    {
        // cache plane and swap lists
        const Fplane2& P = planes[i];
        std::swap(src, dest);
        dest->clear();

        // classify all points relative to plane #i
        float cls[UI_FRUSTUM_SAFE];
        for (u32 j = 0; j < src->size(); j++)
            cls[j] = P.classify((*src)[j].pt);

        // clip everything to this plane
        cls[src->size()] = cls[0];
        src->push_back((*src)[0]);
        Fvector2 dir_pt, dir_uv;
        float denum, t;
        for (u32 j = 0; j < src->size() - 1; j++)
        {
            if ((*src)[j].pt.similar((*src)[j + 1].pt, EPS_S))
                continue;
            if (negative(cls[j]))
            {
                dest->push_back((*src)[j]);
                if (positive(cls[j + 1]))
                {
                    // segment intersects plane
                    dir_pt.sub((*src)[j + 1].pt, (*src)[j].pt);
                    dir_uv.sub((*src)[j + 1].uv, (*src)[j].uv);
                    denum = P.n.dotproduct(dir_pt);
                    if (denum != 0)
                    {
                        t = -cls[j] / denum; // VERIFY(t<=1.f && t>=0);
                        dest->last().pt.mad((*src)[j].pt, dir_pt, t);
                        dest->last().uv.mad((*src)[j].uv, dir_uv, t);
                        dest->inc();
                    }
                }
            }
            else
            {
                // J - outside
                if (negative(cls[j + 1]))
                {
                    // J+1  - inside
                    // segment intersects plane
                    dir_pt.sub((*src)[j + 1].pt, (*src)[j].pt);
                    dir_uv.sub((*src)[j + 1].uv, (*src)[j].uv);
                    denum = P.n.dotproduct(dir_pt);
                    if (denum != 0)
                    {
                        t = -cls[j] / denum; // VERIFY(t<=1.f && t>=0);
                        dest->last().pt.mad((*src)[j].pt, dir_pt, t);
                        dest->last().uv.mad((*src)[j].uv, dir_uv, t);
                        dest->inc();
                    }
                }
            }
        }

        // here we end up with complete polygon in 'dest' which is inside plane #i
        if (dest->size() < 3)
            return 0;
    }
    return dest;
}

void UICore::ClientToScreenScaled(Fvector2& dest, float left, float top) const
{
    if (m_currentPointType != IUIRender::pttLIT)
        dest.set(ClientToScreenScaledX(left), ClientToScreenScaledY(top));
    else
        dest.set(left, top);
}

void UICore::ClientToScreenScaled(Fvector2& src_and_dest) const
{
    if (m_currentPointType != IUIRender::pttLIT)
        src_and_dest.set(ClientToScreenScaledX(src_and_dest.x), ClientToScreenScaledY(src_and_dest.y));
}

void UICore::ClientToScreenScaledWidth(float& src_and_dest) const
{
    if (m_currentPointType != IUIRender::pttLIT)
        src_and_dest /= m_current_scale->x;
}

void UICore::ClientToScreenScaledHeight(float& src_and_dest) const
{
    if (m_currentPointType != IUIRender::pttLIT)
        src_and_dest /= m_current_scale->y;
}

void UICore::AlignPixel(float& src_and_dest) const
{
    if (m_currentPointType != IUIRender::pttLIT)
        src_and_dest = (float)iFloor(src_and_dest);
}

void UICore::PushScissor(const Frect& r_tgt, bool overlapped)
{
    if (UI().m_currentPointType == IUIRender::pttLIT)
        return;

    Frect r_top = {0.0f, 0.0f, UI_BASE_WIDTH, UI_BASE_HEIGHT};
    Frect result = r_tgt;
    if (!m_Scissors.empty() && !overlapped)
    {
        r_top = m_Scissors.top();
    }
    if (!result.intersection(r_top, r_tgt))
        result.set(0.0f, 0.0f, 0.0f, 0.0f);

    if (!(result.x1 >= 0 && result.y1 >= 0 && result.x2 <= UI_BASE_WIDTH && result.y2 <= UI_BASE_HEIGHT))
    {
        Msg("! r_tgt [%.3f][%.3f][%.3f][%.3f]", r_tgt.x1, r_tgt.y1, r_tgt.x2, r_tgt.y2);
        Msg("! result [%.3f][%.3f][%.3f][%.3f]", result.x1, result.y1, result.x2, result.y2);
        VERIFY(result.x1 >= 0 && result.y1 >= 0 && result.x2 <= UI_BASE_WIDTH && result.y2 <= UI_BASE_HEIGHT);
    }
    m_Scissors.push(result);

    result.lt.x = ClientToScreenScaledX(result.lt.x);
    result.lt.y = ClientToScreenScaledY(result.lt.y);
    result.rb.x = ClientToScreenScaledX(result.rb.x);
    result.rb.y = ClientToScreenScaledY(result.rb.y);

    Irect r;
    r.x1 = iFloor(result.x1);
    r.x2 = iFloor(result.x2 + 0.5f);
    r.y1 = iFloor(result.y1);
    r.y2 = iFloor(result.y2 + 0.5f);
    GEnv.UIRender->SetScissor(&r);
}

void UICore::PopScissor()
{
    if (UI().m_currentPointType == IUIRender::pttLIT)
        return;

    VERIFY(!m_Scissors.empty());
    m_Scissors.pop();

    if (m_Scissors.empty())
        GEnv.UIRender->SetScissor(NULL);
    else
    {
        const Frect& top = m_Scissors.top();
        Irect tgt;
        tgt.lt.x = iFloor(ClientToScreenScaledX(top.lt.x));
        tgt.lt.y = iFloor(ClientToScreenScaledY(top.lt.y));
        tgt.rb.x = iFloor(ClientToScreenScaledX(top.rb.x));
        tgt.rb.y = iFloor(ClientToScreenScaledY(top.rb.y));

        GEnv.UIRender->SetScissor(&tgt);
    }
}

UICore::UICore()
{
    if (!GEnv.isDedicatedServer)
    {
        m_pUICursor = xr_new<CUICursor>();
        m_pFontManager = xr_new<CFontManager>();
    }
    else
    {
        m_pUICursor = nullptr;
        m_pFontManager = nullptr;
    }
    m_bPostprocess = false;

    OnDeviceReset();
    OnUIReset();

    m_current_scale = &m_scale_;
    g_current_font_scale.set(1.0f, 1.0f);
    m_currentPointType = IUIRender::pttTL;
}

void UICore::OnDeviceReset()
{
    m_scale_.set(float(Device.dwWidth) / UI_BASE_WIDTH, float(Device.dwHeight) / UI_BASE_HEIGHT);

    m_2DFrustum.CreateFromRect(Frect().set(0.0f, 0.0f, float(Device.dwWidth), float(Device.dwHeight)));
}

void UICore::OnUIReset()
{
    CUIXmlInitBase::DeleteColorDefs();
    CUITextureMaster::FreeTexInfo();

    ReadTextureInfo();
    CUIXmlInitBase::InitColorDefs();
}

UICore::~UICore()
{
    xr_delete(m_pFontManager);
    xr_delete(m_pUICursor);
    CUIXmlInitBase::DeleteColorDefs();
    CUITextureMaster::FreeTexInfo();
}

void UICore::ReadTextureInfo()
{
    string_path buf;
    FS_FileSet files;

    const auto ParseFileSet = [&](pcstr path)
    {
        FS.file_list(files, "$game_config$", FS_ListFiles,
            strconcat(sizeof(buf), buf, path, DELIMITER "textures_descr" DELIMITER "*.xml")
        );
        for (const auto& file : files)
        {
            string_path path, name;
            _splitpath(file.name.c_str(), nullptr, path, name, nullptr);
            xr_strcat(name, ".xml");
            path[xr_strlen(path) - 1] = '\0'; // cut the latest '\\'

            CUITextureMaster::ParseShTexInfo(path, name);
        }
    };

    ParseFileSet(UI_PATH_DEFAULT);

    if (0 != xr_strcmp(UI_PATH, UI_PATH_DEFAULT))
        ParseFileSet(UI_PATH);

    if (pSettings->section_exist("texture_desc"))
    {
        string256 single_item;

        cpcstr itemsList = pSettings->r_string("texture_desc", "files");
        const u32 itemsCount = _GetItemCount(itemsList);

        for (u32 i = 0; i < itemsCount; i++)
        {
            _GetItem(itemsList, i, single_item);
            xr_strcat(single_item, ".xml");
            CUITextureMaster::ParseShTexInfo(single_item);
        }
    }

    if (ShadowOfChernobylMode)
    {
        // SHoC's atlas descriptors are at config\ui\, not under textures_descr\.
        // Load them so their atlas IDs are registered. Some XMLs (ui_hud.xml) may not
        // be in texture-descriptor format and will silently skip via the non-fatal
        // ParseShTexInfo path.
        CUITextureMaster::ParseShTexInfo("ui_old_textures.xml");
        CUITextureMaster::ParseShTexInfo("ui_hud.xml");
        CUITextureMaster::ParseShTexInfo("ui_npc_unique.xml");
        CUITextureMaster::ParseShTexInfo("ui_npc_monster.xml");
        CUITextureMaster::ParseShTexInfo("ui_icons_npc.xml");
        CUITextureMaster::ParseShTexInfo("ui_iconstotal.xml");
        CUITextureMaster::ParseShTexInfo("ui_icons_map.xml");
        CUITextureMaster::ParseShTexInfo("ui_mp_icon_rank.xml");
        // ui_common.xml holds most of the shared SHoC widget atlases (sliders,
        // scroll bars, tab buttons, checkboxes ...). Without it the engine falls
        // back to CoP-only ids (ui_inGame2_opt_slider_*) and renders dummies.
        CUITextureMaster::ParseShTexInfo("ui", "ui_common.xml");

        // SHoC stores UI elements as separate per-part DDS files rather than packed atlases.
        // Read each DDS header to get the real pixel dimensions, then register an atlas entry
        // for the ID that OpenXRay's CUIFrameWindow / CUIFrameLineWnd / CUIScrollBar expect.
        // When SHoC has no _e (FrameLine end), reuse _b. For scrollbars that have only one
        // monolithic DDS per element, reuse the same file for back/b/e and state variants.
        struct { pcstr dds; pcstr id; } const mappings[] =
        {
            // PDA inner frame (frame2) — 9 parts for CUIFrameWindow
            { "ui\\ui_pda_frame2_back", "ui_pda2_fr2_back" },
            { "ui\\ui_pda_frame2_b",    "ui_pda2_fr2_b"    },
            { "ui\\ui_pda_frame2_b",    "ui_pda2_fr2_e"    },
            { "ui\\ui_pda_frame2_l",    "ui_pda2_fr2_l"    },
            { "ui\\ui_pda_frame2_r",    "ui_pda2_fr2_r"    },
            { "ui\\ui_pda_frame2_t",    "ui_pda2_fr2_t"    },
            { "ui\\ui_pda_frame2_lt",   "ui_pda2_fr2_lt"   },
            { "ui\\ui_pda_frame2_rt",   "ui_pda2_fr2_rt"   },
            { "ui\\ui_pda_frame2_lb",   "ui_pda2_fr2_lb"   },
            { "ui\\ui_pda_frame2_rb",   "ui_pda2_fr2_rb"   },

            // PDA outer frame — same scheme, ui_pda_frame_* files
            { "ui\\ui_pda_frame_back",  "ui_pda2_fr_back"  },
            { "ui\\ui_pda_frame_b",     "ui_pda2_fr_b"     },
            { "ui\\ui_pda_frame_b",     "ui_pda2_fr_e"     },
            { "ui\\ui_pda_frame_l",     "ui_pda2_fr_l"     },
            { "ui\\ui_pda_frame_r",     "ui_pda2_fr_r"     },
            { "ui\\ui_pda_frame_t",     "ui_pda2_fr_t"     },
            { "ui\\ui_pda_frame_lt",    "ui_pda2_fr_lt"    },
            { "ui\\ui_pda_frame_rt",    "ui_pda2_fr_rt"    },
            { "ui\\ui_pda_frame_lb",    "ui_pda2_fr_lb"    },
            { "ui\\ui_pda_frame_rb",    "ui_pda2_fr_rb"    },

            // Center delimiter — CUIFrameWindow expects all 9 subparts; reuse sub-frame DDS
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_back" },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_b"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_l"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_r"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_t"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_lt"   },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_rt"   },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_lb"   },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_ranking_rb"   },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_back" },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_b"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_l"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_r"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_t"    },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_lt"   },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_rt"   },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_lb"   },
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_fr_delimiter_fraction_rb"   },

            // Scrollbar IDs INCLUDE the "ui\" prefix because scroll_bar.xml references
            // them as "ui\ui_scb_..." and CUIFrameLineWnd appends _back/_b/_e to that base.
            // Vertical scrollbar track (FrameLine: back/b/e)
            { "ui\\ui_scb_back_v",       "ui\\ui_scb_back_v_back" },
            { "ui\\ui_scb_back_v",       "ui\\ui_scb_back_v_b"    },
            { "ui\\ui_scb_back_v",       "ui\\ui_scb_back_v_e"    },

            // Vertical scroll box — FrameLine + state variants (e/d/t/h) for FixedScrollBar
            { "ui\\ui_scb_scroll_box_v", "ui\\ui_scb_scroll_box_v_back" },
            { "ui\\ui_scb_scroll_box_v", "ui\\ui_scb_scroll_box_v_b"    },
            { "ui\\ui_scb_scroll_box_v", "ui\\ui_scb_scroll_box_v_e"    },
            { "ui\\ui_scb_scroll_box_v", "ui\\ui_scb_scroll_box_v_d"    },
            { "ui\\ui_scb_scroll_box_v", "ui\\ui_scb_scroll_box_v_t"    },
            { "ui\\ui_scb_scroll_box_v", "ui\\ui_scb_scroll_box_v_h"    },

            // Horizontal scrollbar track
            { "ui\\ui_scb_back",         "ui\\ui_scb_back_back" },
            { "ui\\ui_scb_back",         "ui\\ui_scb_back_b"    },
            { "ui\\ui_scb_back",         "ui\\ui_scb_back_e"    },

            // Horizontal scroll box — FrameLine + state variants
            { "ui\\ui_scb_scroll_box",   "ui\\ui_scb_scroll_box_back" },
            { "ui\\ui_scb_scroll_box",   "ui\\ui_scb_scroll_box_b"    },
            { "ui\\ui_scb_scroll_box",   "ui\\ui_scb_scroll_box_e"    },
            { "ui\\ui_scb_scroll_box",   "ui\\ui_scb_scroll_box_d"    },
            { "ui\\ui_scb_scroll_box",   "ui\\ui_scb_scroll_box_t"    },
            { "ui\\ui_scb_scroll_box",   "ui\\ui_scb_scroll_box_h"    },

            // Horizontal split divider (FrameLine 3-part) — SHoC has ui_pda_horizontal_line
            { "ui\\ui_pda_horizontal_line", "ui_pda2_split_back" },
            { "ui\\ui_pda_horizontal_line", "ui_pda2_split_b"    },
            { "ui\\ui_pda_horizontal_line", "ui_pda2_split_e"    },

            // Map frame inside the PDA (CUIFrameWindow 9-part) — reuse ui_pda_frame_sub_*
            { "ui\\ui_pda_frame_sub_back", "ui_pda2_mapframe_back" },
            { "ui\\ui_pda_frame_sub_b",    "ui_pda2_mapframe_b"    },
            { "ui\\ui_pda_frame_sub_l",    "ui_pda2_mapframe_l"    },
            { "ui\\ui_pda_frame_sub_r",    "ui_pda2_mapframe_r"    },
            { "ui\\ui_pda_frame_sub_t",    "ui_pda2_mapframe_t"    },
            { "ui\\ui_pda_frame_sub_lt",   "ui_pda2_mapframe_lt"   },
            { "ui\\ui_pda_frame_sub_rt",   "ui_pda2_mapframe_rt"   },
            { "ui\\ui_pda_frame_sub_lb",   "ui_pda2_mapframe_lb"   },
            { "ui\\ui_pda_frame_sub_rb",   "ui_pda2_mapframe_rb"   },

            // Center delimiter ranking_03 — single vertical line as visual stand-in
            { "ui\\ui_pda_vertical_line_l_l", "ui_pda2_delimiter_03" },

            // Map markers — reuse SHoC PDA news/mission icons as visual stand-ins
            { "ui\\ui_pda_icon_news_jobs",          "ui_pda2_trader"          },
            { "ui\\ui_pda_icon_news_jobs",          "ui_pda2_guide"           },
            { "ui\\ui_pda_icon_news_jobs",          "ui_pda2_mechanic"        },
            { "ui\\ui_pda_icon_mission_in_progress","ui_pda2_secondary_task"  },
            { "ui\\ui_pda_icon_mission_in_progress","ui_pda2_mtask_overlay"   },
            { "ui\\ui_pda_contacts_online",         "ui_pda2_squad_leader"    },
            { "ui\\ui_pda_icon_news_misc",          "ui_pda2_exit_point"      },
            { "ui\\ui_pda_icon_news_knowledge",     "ui_pda2_important1"      },

            // Character portrait frame overlays — reuse sub-frame background
            { "ui\\ui_pda_frame_sub_back", "ui_fm_over_chars"    },
            { "ui\\ui_pda_frame_sub_back", "ui_fm_base_fraction" },
        };
        for (const auto& m : mappings)
        {
            string_path fn;
            if (!FS.exist(fn, "$game_textures$", m.dds, ".dds"))
                continue;
            IReader* r = FS.r_open(fn);
            if (!r)
                continue;
            if (r->length() >= 20)
            {
                struct { u32 magic, size, flags, height, width; } hdr;
                r->r(&hdr, sizeof(hdr));
                if (hdr.magic == 0x20534444u) // "DDS "
                {
                    Frect rect;
                    rect.set(0.0f, 0.0f, float(hdr.width), float(hdr.height));
                    CUITextureMaster::RegisterTexture(m.id, m.dds, rect);
                }
            }
            FS.r_close(r);
        }

        // Rank icons (sub-rects on ui\ui_hud.dds — inherited atlas layout from CS/CoP).
        // Coords come from CS/CoP's textures_descr/ui_hud.xml; SHoC's ui_hud.dds shares
        // the same layout. Registered here because SHoC's atlas descriptors lack these IDs.
        struct { pcstr id; float x, y, w, h; } const hud_rects[] =
        {
            { "pre_novice",  0.0f, 728.0f, 46.0f, 47.0f },
            { "novice",      0.0f, 775.0f, 46.0f, 47.0f },
            { "experienced", 0.0f, 822.0f, 46.0f, 47.0f },
            { "veteran",     0.0f, 869.0f, 46.0f, 47.0f },
            { "master",    667.0f,  40.0f, 46.0f, 47.0f },
        };
        for (const auto& h : hud_rects)
        {
            Frect rect;
            rect.set(h.x, h.y, h.x + h.w, h.y + h.h);
            CUITextureMaster::RegisterTexture(h.id, "ui\\ui_hud", rect);
        }

        // Button stubs — copy atlas rects from existing SHoC button definitions to the
        // CoP-specific IDs that don't exist in SHoC. State suffix (e/d/t/h) is preserved.
        struct { pcstr target_prefix; pcstr source; } const button_aliases[] =
        {
            // Map navigation buttons → ui_button_ordinary_<state>
            { "ui_mnav_bt_legend_",   "ui_button_ordinary_" },
            { "ui_mnav_bt_up_",       "ui_button_ordinary_" },
            { "ui_mnav_bt_down_",     "ui_button_ordinary_" },
            { "ui_mnav_bt_left_",     "ui_button_ordinary_" },
            { "ui_mnav_bt_right_",    "ui_button_ordinary_" },
            { "ui_mnav_bt_center_",   "ui_button_ordinary_" },
            { "ui_mnav_bt_zoom_in_",  "ui_button_ordinary_" },
            { "ui_mnav_bt_zoom_out_", "ui_button_ordinary_" },
            { "ui_mnav_bt_zmreset_",  "ui_button_ordinary_" },

            // Task tab buttons
            { "ui_task_bt_center_", "ui_button_ordinary_" },
            { "ui_task_bt_tslist_", "ui_button_ordinary_" },
            { "ui_task_bt_close_",  "ui_button_ordinary_" },

            // Date filter arrows → spiner button (smaller, more arrow-like)
            { "ui_date_bt_left_",  "ui_spiner_button_t_" },
            { "ui_date_bt_right_", "ui_spiner_button_b_" },
        };
        pcstr const states[] = { "e", "d", "t", "h" };
        for (const auto& a : button_aliases)
        {
            for (pcstr s : states)
            {
                string128 src, dst;
                xr_sprintf(src, "%s%s", a.source, s);
                xr_sprintf(dst, "%s%s", a.target_prefix, s);
                TEX_INFO info;
                if (CUITextureMaster::FindItem(src, info))
                    CUITextureMaster::RegisterTexture(dst, info.file, info.rect);
            }
        }

        // Date filter background tile → reuse listline_back rect
        {
            TEX_INFO info;
            if (CUITextureMaster::FindItem("ui_listline_back", info))
                CUITextureMaster::RegisterTexture("ui_date_back_tile", info.file, info.rect);
        }

        // Scrollbar track and box textures from SHoC's scroll_bar.xml are single atlas
        // entries (ui_common.xml), but CUIFrameLineWnd needs <id>_back/_b/_e.
        // The SHoC textures are uniform tiles, so the same rect serves for all three parts.
        pcstr const frameline_bases[] =
        {
            "ui_scroll_back",
            "ui_scroll_box",
            "ui_scroll_PDA_back",
            "ui_scroll_PDA_back_hor",
            "ui_scroll_PDA_box_e",
        };
        for (pcstr base : frameline_bases)
        {
            TEX_INFO info;
            if (!CUITextureMaster::FindItem(base, info))
                continue;

            string128 back, first, second;
            xr_sprintf(back, "%s_back", base);
            xr_sprintf(first, "%s_b", base);
            xr_sprintf(second, "%s_e", base);
            if (CUITextureMaster::ItemExist(back))
                continue;

            CUITextureMaster::RegisterTexture(back, info.file, info.rect);
            CUITextureMaster::RegisterTexture(first, info.file, info.rect);
            CUITextureMaster::RegisterTexture(second, info.file, info.rect);
        }

        // State variants (disabled/touched/highlighted) of the PDA scroll boxes
        struct { pcstr id; pcstr source; } const scroll_box_states[] =
        {
            { "ui_scroll_PDA_box_e_d",     "ui_scroll_PDA_box_e" },
            { "ui_scroll_PDA_box_e_t",     "ui_scroll_PDA_box_t" },
            { "ui_scroll_PDA_box_e_h",     "ui_scroll_PDA_box_h" },
            // scroll_bar.xml uses the horizontal track texture for the horizontal box
            { "ui_scroll_PDA_back_hor_d",  "ui_scroll_PDA_back_hor" },
            { "ui_scroll_PDA_back_hor_t",  "ui_scroll_PDA_back_hor" },
            { "ui_scroll_PDA_back_hor_h",  "ui_scroll_PDA_back_hor" },
        };
        for (const auto& s : scroll_box_states)
        {
            TEX_INFO info;
            if (!CUITextureMaster::ItemExist(s.id) && CUITextureMaster::FindItem(s.source, info))
                CUITextureMaster::RegisterTexture(s.id, info.file, info.rect);
        }
    }
}

void UICore::pp_start()
{
    m_bPostprocess = true;

    m_pp_scale_.set(float(Device.dwWidth) / float(UI_BASE_WIDTH),
        float(Device.dwHeight) / float(UI_BASE_HEIGHT));
    m_2DFrustumPP.CreateFromRect(Frect().set(0.0f, 0.0f, float(Device.dwWidth),
        float(Device.dwHeight)));

    m_current_scale = &m_pp_scale_;

    g_current_font_scale.set(float(Device.dwWidth) / float(Device.dwWidth),
        float(Device.dwHeight) / float(Device.dwHeight));
}

void UICore::pp_stop()
{
    m_bPostprocess = false;
    m_current_scale = &m_scale_;
    g_current_font_scale.set(1.0f, 1.0f);
}

void UICore::RenderFont()
{
    ZoneScoped;
    Font().Render();
}

bool UICore::is_widescreen()
{
    return (Device.dwWidth) / float(Device.dwHeight) > (UI_BASE_WIDTH / UI_BASE_HEIGHT + 0.01f);
}

float UICore::get_current_kx()
{
    float h = float(Device.dwHeight);
    float w = float(Device.dwWidth);

    float res = (h / w) / (UI_BASE_HEIGHT / UI_BASE_WIDTH);
    return res;
}

shared_str UICore::get_xml_name(pcstr path, pcstr fn)
{
    string_path str;
    if (!is_widescreen())
    {
        xr_sprintf(str, "%s", fn);
        if (NULL == strext(fn))
            xr_strcat(str, ".xml");
    }
    else
    {
        if (strext(fn))
        {
            xr_strcpy(str, fn);
            *strext(str) = 0;
            xr_strcat(str, "_16.xml");
        }
        else
            xr_sprintf(str, "%s_16", fn);

        string_path str_;
        if (!FS.exist(str_, "$game_config$", path, str))
        {
            xr_sprintf(str, "%s", fn);
            if (nullptr == strext(fn))
                xr_strcat(str, ".xml");
        }
    }
    return str;
}
