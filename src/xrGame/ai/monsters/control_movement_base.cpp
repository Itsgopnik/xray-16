#include "StdAfx.h"
#include "control_movement_base.h"
#include "control_animation_base.h"
#include "control_direction_base.h"
#include "basemonster/base_monster.h"
#include "monster_velocity_space.h"
#include "detail_path_manager.h"

using namespace MonsterMovement;

void CControlMovementBase::reinit()
{
    inherited::reinit();

    stop();

    m_man->capture(this, ControlCom::eControlMovement);
}

void CControlMovementBase::load(LPCSTR section)
{
    // Lines added after SoC (WalkSmelling, WalkGrowl, RunAttack) are missing in old configs;
    // a zero velocity makes path building fail, so use the closest basic velocity instead.
    load_velocity(section, "Velocity_Stand", eVelocityParameterStand);
    load_velocity(section, "Velocity_WalkFwdNormal", eVelocityParameterWalkNormal);
    load_velocity(section, "Velocity_WalkSmelling", eVelocityParameterWalkSmelling, "Velocity_WalkFwdNormal");
    load_velocity(section, "Velocity_WalkGrowl", eVelocityParameterWalkGrowl, "Velocity_WalkFwdNormal");
    load_velocity(section, "Velocity_RunFwdNormal", eVelocityParameterRunNormal);
    load_velocity(section, "Velocity_WalkFwdDamaged", eVelocityParameterWalkDamaged);
    load_velocity(section, "Velocity_RunFwdDamaged", eVelocityParameterRunDamaged);
    load_velocity(section, "Velocity_Steal", eVelocityParameterSteal, "Velocity_WalkFwdNormal");
    load_velocity(section, "Velocity_Drag", eVelocityParameterDrag, "Velocity_WalkFwdNormal");
    load_velocity(section, "Velocity_RunAttack", eVelocityParameterRunAttack, "Velocity_RunFwdNormal");

    // add idle velocity
    SVelocityParam velocity_param;
    m_velocities.insert(std::make_pair(eVelocityParameterIdle, velocity_param));
    m_man->path_builder().detail().add_velocity(
        eVelocityParameterIdle, CDetailPathManager::STravelParams(velocity_param.velocity.linear,
                                    velocity_param.velocity.angular_path, velocity_param.velocity.angular_real));
}

void CControlMovementBase::load_velocity(LPCSTR section, LPCSTR line, u32 velocity_id, LPCSTR fallback_line)
{
    SVelocityParam velocity_param;
    if (pSettings->line_exist(section, line))
        velocity_param.Load(section, line);
    else if (fallback_line && pSettings->line_exist(section, fallback_line))
    {
        velocity_param.Load(section, fallback_line);
#ifndef MASTER_GOLD
        static xr_set<xr_string> reported;
        if (reported.emplace(xr_string(section) + "/" + line).second)
            Msg("~ [%s] has no %s, using %s", section, line, fallback_line);
#endif
    }
    m_velocities.insert(std::make_pair(velocity_id, velocity_param));

    m_man->path_builder().detail().add_velocity(
        velocity_id, CDetailPathManager::STravelParams(velocity_param.velocity.linear,
                         velocity_param.velocity.angular_path, velocity_param.velocity.angular_real));
}

SVelocityParam& CControlMovementBase::get_velocity(u32 velocity_id)
{
    auto it = m_velocities.find(velocity_id);
    VERIFY(it != m_velocities.end());

    return it->second;
}

void CControlMovementBase::update_frame()
{
    SControlMovementData* ctrl_data = (SControlMovementData*)m_man->data(this, ControlCom::eControlMovement);
    if (!ctrl_data)
        return;

    ctrl_data->velocity_target = m_velocity;
    ctrl_data->acc = m_accel;
}

void CControlMovementBase::set_velocity(float val, bool max_acc)
{
    m_velocity = val;
    if (max_acc)
        m_accel = flt_max;
    else
    {
        m_accel = ((m_man->movement().velocity_current() > m_velocity) ? m_object->anim().accel_get(eAV_Braking) :
                                                                         m_object->anim().accel_get(eAV_Accel));
    }
}

void CControlMovementBase::stop()
{
    m_velocity = 0.f;
    m_accel = flt_max;
}

void CControlMovementBase::stop_accel()
{
    m_velocity = 0.f;
    m_accel = ((m_man->movement().velocity_current() > m_velocity) ? m_object->anim().accel_get(eAV_Braking) : flt_max);
}

float CControlMovementBase::get_velocity_from_path()
{
    if (m_man->path_builder().path().empty())
        return 0.f;
    if (!m_man->path_builder().enabled())
        return 0.f;

    // get target velocity from path
    float velocity = 0.f;

    CDetailPathManager& detail = m_man->path_builder().detail();

    u32 cur_point_velocity_index = detail.path()[detail.curr_travel_point_index()].velocity;
    u32 next_point_velocity_index = u32(-1);

    if (detail.path().size() > detail.curr_travel_point_index() + 1)
        next_point_velocity_index = detail.path()[detail.curr_travel_point_index() + 1].velocity;

    const CDetailPathManager::STravelParams& current_velocity = detail.velocity(cur_point_velocity_index);
    if (fis_zero(current_velocity.linear_velocity) && (next_point_velocity_index != u32(-1)))
    {
        const CDetailPathManager::STravelParams& next_velocity = detail.velocity(next_point_velocity_index);
        velocity = _abs(next_velocity.linear_velocity);
        m_object->dir().set_heading_speed(next_velocity.real_angular_velocity);
    }
    else
    {
        velocity = _abs(current_velocity.linear_velocity);
        m_object->dir().set_heading_speed(current_velocity.real_angular_velocity);
    }

    return velocity;
    // m_accel		= ((m_man->movement().velocity_current() > velocity) ?
    //	m_object->anim().accel_get(eAV_Braking) :
    // m_object->anim().accel_get(eAV_Accel));
}
