////////////////////////////////////////////////////////////////////////////
//	Module 		: xrServer_Objects_ALife_Monsters_script.cpp
//	Created 	: 19.09.2002
//  Modified 	: 04.06.2003
//	Author		: Dmitriy Iassenev
//	Description : Server monsters for ALife simulator, script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"

#include "specific_character.h"

#include "xrServer_Objects_ALife_Monsters.h"
#include "xrServer_script_macroses.h"

void CSE_ALifeTraderAbstract::script_register(lua_State* luaState)
{
    using namespace luabind;

    module(luaState)
    [
        class_<CSE_ALifeTraderAbstract>("cse_alife_trader_abstract")
            //			.def(		constructor<pcstr>())
            .def("community", &CSE_ALifeTraderAbstract::CommunityName)
            .def("profile_name", +[](CSE_ALifeTraderAbstract* ta) { return ta->character_profile().c_str(); })
            .def("set_profile_name", +[](CSE_ALifeTraderAbstract* ta, const pcstr str) { ta->set_character_profile(str); })
            .def("character_name", +[](CSE_ALifeTraderAbstract* ta) { return ta->m_character_name.c_str(); })
            .def("set_character_name", +[](CSE_ALifeTraderAbstract* ta, const pcstr str) { ta->m_character_name = str; })
            .def("rank", &CSE_ALifeTraderAbstract::Rank)
            .def("set_rank", &CSE_ALifeTraderAbstract::SetRank)
            .def("reputation", &CSE_ALifeTraderAbstract::Reputation)
            .def("character_icon", +[](CSE_ALifeTraderAbstract* ta)
            {
                ta->specific_character();
                if (ta->m_icon_name.empty())
                {
                    CSpecificCharacter selected_char;
                    selected_char.Load(ta->m_SpecificCharacter);
                    ta->m_icon_name = selected_char.IconName();
                }
                return ta->m_icon_name.c_str();
            })
    ];
}

void CSE_ALifeTrader::script_register(lua_State* luaState)
{
    using namespace luabind;

    module(luaState)
    [
        luabind_class_dynamic_alife2(
            CSE_ALifeTrader, "cse_alife_trader", CSE_ALifeDynamicObjectVisual, CSE_ALifeTraderAbstract)
    ];
}

void CSE_ALifeCustomZone::script_register(lua_State* luaState)
{
    using namespace luabind;

    module(luaState)
    [
        luabind_class_dynamic_alife2(
            CSE_ALifeCustomZone, "cse_custom_zone", CSE_ALifeDynamicObject, CSE_Shape)
    ];
}

#ifdef XRGAME_EXPORTS
namespace
{
// SoC 1.0006 exported spawn_artefacts() to Lua (se_zones.script calls it).
// Spawning is only valid for offline zones.
void script_spawn_artefacts(CSE_ALifeAnomalousZone* zone)
{
    if (zone->m_bOnline)
    {
        Msg("! [%s] spawn_artefacts called for an online zone, ignored", zone->name_replace());
        return;
    }
    zone->spawn_artefacts();
}

template <typename Class>
Class& register_shoc_zone_methods(Class& instance)
{
    if (ShadowOfChernobylMode)
        instance.def("spawn_artefacts", &script_spawn_artefacts);
    return instance;
}
} // namespace
#endif

void CSE_ALifeAnomalousZone::script_register(lua_State* luaState)
{
    using namespace luabind;

    module(luaState)
    [
#ifdef XRGAME_EXPORTS
        register_shoc_zone_methods(
            luabind_class_dynamic_alife1(CSE_ALifeAnomalousZone, "cse_anomalous_zone", CSE_ALifeCustomZone))
#else
        luabind_class_dynamic_alife1(CSE_ALifeAnomalousZone, "cse_anomalous_zone", CSE_ALifeCustomZone)
#endif
    ];
}

void CSE_ALifeMonsterRat::script_register(lua_State* luaState)
{
    using namespace luabind;

    module(luaState)
    [
        luabind_class_monster2(
            CSE_ALifeMonsterRat, "cse_alife_monster_rat", CSE_ALifeMonsterAbstract, CSE_ALifeInventoryItem)
    ];
}
