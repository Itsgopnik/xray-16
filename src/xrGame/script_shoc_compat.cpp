////////////////////////////////////////////////////////////////////////////
//  Module      : script_shoc_compat.cpp
//  Description : Script API shims for Shadow of Chernobyl scripts (#392)
////////////////////////////////////////////////////////////////////////////

// SoC 1.0006 scripts call a few engine functions that were removed or renamed
// in CS/CoP. They are registered only in SoC mode, and every distinct call site
// is logged once ("~ [shoc-shim] ..."), so the remaining uses show up in test logs.
// Only functions with live callers in vanilla SoC scripts belong here; see
// doc/design/shoc-compatibility.md for the analysis.

#include "pch_script.h"
#include "script_game_object.h"
#include "script_game_object_impl.h"
#include "Actor.h"
#include "Level.h"
#include "CustomMonster.h"
#include "movement_manager.h"
#include "restricted_object.h"
#include "xrScriptEngine/script_engine.hpp"
#include "ui/UISHoCRankingWnd.h"

namespace
{
void log_shim_call(lua_State* L, pcstr name)
{
    string512 site = "unknown";
    lua_Debug ar;
    if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar))
        xr_sprintf(site, "%s:%d", ar.short_src, ar.currentline);

    static xr_set<xr_string> reported;
    if (reported.emplace(xr_string(name) + "@" + site).second)
        Msg("~ [shoc-shim] %s called from %s", name, site);
}

// level.actor() was removed after SoC; db.actor / level.object_by_id replace it
CScriptGameObject* level_actor(lua_State* L)
{
    log_shim_call(L, "level.actor");
    if (!g_pGameLevel)
        return nullptr;
    CActor* actor = Actor();
    return actor ? actor->lua_game_object() : nullptr;
}

// SoC: level vertex nearest to the object's position that its restrictions allow.
// Used by xr_attendant (follower scheme) when the follower stands on an inaccessible vertex.
u32 nearest_accessible_position(CScriptGameObject* self, lua_State* L)
{
    log_shim_call(L, "game_object:nearest_accessible_position");
    CCustomMonster* monster = smart_cast<CCustomMonster*>(&self->object());
    if (!monster)
    {
        GEnv.ScriptEngine->script_log(LuaMessageType::Error,
            "CRestrictedObject : cannot access class member nearest_accessible_position!");
        return u32(-1);
    }
    const Fvector& position = monster->Position();
    CRestrictedObject& restrictions = monster->movement().restrictions();
    if (restrictions.accessible(position))
        return monster->ai_location().level_vertex_id();
    Fvector result;
    return restrictions.accessible_nearest(position, result);
}
} // namespace

luabind::class_<CScriptGameObject>& script_register_game_object_shoc_compat(luabind::class_<CScriptGameObject>& instance)
{
    if (ShadowOfChernobylMode)
        instance.def("nearest_accessible_position", &nearest_accessible_position);
    return instance;
}

void script_register_level_shoc_compat(lua_State* luaState)
{
    if (!ShadowOfChernobylMode)
        return;

    // Runs on every script engine start, i.e. before ALife (re)registers the stalkers
    shoc_ranking_clear();

    using namespace luabind;
    module(luaState, "level")
    [
        def("actor", &level_actor)
    ];
    // Called from se_stalker/bind_stalker (guarded with ~= nil) to fill the PDA ranking
    module(luaState, "actor_stats")
    [
        def("add_to_ranking", &shoc_ranking_add),
        def("remove_from_ranking", &shoc_ranking_remove)
    ];
}
