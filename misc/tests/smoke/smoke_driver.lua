-- OpenXRay smoke test driver.
--
-- Loaded by the engine through "-test_script <this file>" (non-MASTER_GOLD builds)
-- on every script engine (re)start, so it is executed again after ALife start
-- and after every game load. State that must survive is kept in files.
--
-- Environment (set by run_smoke.sh):
--   XRAY_SMOKE_DIR    directory for state.txt and results.txt
--   XRAY_SMOKE_PHASE  menu | play | load | dogs | pda | music
--
-- Results are appended to results.txt as "PASS|FAIL <step> <details>".

local smoke_dir = os.getenv("XRAY_SMOKE_DIR")
local phase = os.getenv("XRAY_SMOKE_PHASE")
if not smoke_dir or not phase then
    return
end

local SAVE_NAME = "smoke_test"

local function say(text)
    -- wall clock and game clock, to tell slow frames from slow game time
    (log1 or log)(string.format("[smoke t=%d gt=%d] %s", os.time(), time_global() / 1000, text))
    flush()
end

local function report(ok, step, details)
    local f = io.open(smoke_dir .. "/results.txt", "a")
    if f then
        f:write(string.format("%s %s %s\n", ok and "PASS" or "FAIL", step, details or ""))
        f:close()
    end
    say(string.format("%s %s %s", ok and "PASS" or "FAIL", step, details or ""))
end

local function read_state()
    local state = {}
    local f = io.open(smoke_dir .. "/state.txt", "r")
    if f then
        for line in f:lines() do
            local k, v = line:match("^(%S+)%s*=%s*(.-)$")
            if k then
                state[k] = v
            end
        end
        f:close()
    end
    return state
end

local function write_state(state)
    local f = io.open(smoke_dir .. "/state.txt", "w")
    for k, v in pairs(state) do
        f:write(k .. " = " .. tostring(v) .. "\n")
    end
    f:close()
end

local function console(cmd)
    say("console: " .. cmd)
    get_console():execute(cmd)
end

local function save_file_exists()
    local path = getFS():update_path("$game_saves$", SAVE_NAME .. ".sav")
    local f = io.open(path, "rb")
    if f then
        f:close()
        return true
    end
    return false
end

-- Tiny step sequencer driven by level.add_call / main menu Update
local steps, step_index, step_started = {}, 1, nil

local function run_steps()
    local step = steps[step_index]
    if not step then
        return true
    end
    local now = time_global()
    step_started = step_started or now
    local ok, done = pcall(step, now - step_started)
    if not ok then
        report(false, "driver", "script error: " .. tostring(done))
        console("quit")
        steps = {}
        return true
    end
    if done then
        step_index = step_index + 1
        step_started = nil
    end
    return false
end

local function wait(ms)
    return function(elapsed) return elapsed >= ms end
end

local function actor()
    return db.actor
end

local function find_enemy()
    local a = actor()
    local best, best_dist
    for id = 1, 65534 do
        local obj = level.object_by_id(id)
        if obj and id ~= a:id() and (obj:is_monster() or obj:is_stalker()) and obj:alive()
            and obj:relation(a) == game_object.enemy and obj:story_id() == 4294967295 then
            local dist = obj:position():distance_to(a:position())
            if not best or dist < best_dist then
                best, best_dist = obj, dist
            end
        end
    end
    return best, best_dist
end

-- Is the object dead? Checks the online object first, then the server entity.
local function verify_dead(id, step)
    local obj = level.object_by_id(id)
    if obj then
        report(not obj:alive(), step, string.format("id=%d %s online alive=%s", id, obj:name(), tostring(obj:alive())))
        return
    end
    local se = alife():object(id)
    if not se then
        report(true, step, string.format("id=%d removed from alife (corpse cleaned up)", id))
        return
    end
    report(not se:alive(), step, string.format("id=%d %s offline alive=%s", id, se:name(), tostring(se:alive())))
end

local function play_steps()
    local state = read_state()
    if state.stage == "saved" then
        -- Second actor bind in the same process: the in-process load finished
        return {
            wait(10000),
            function()
                report(true, "load_inprocess", "actor spawned after 'load " .. SAVE_NAME .. "'")
                verify_dead(tonumber(state.target_id), "enemy_dead_after_inprocess_load")
                state.stage = "done"
                write_state(state)
                return true
            end,
            wait(2000),
            function() console("quit") return true end,
        }
    end

    local target_id
    local save_attempts = 0
    return {
        wait(20000),
        function()
            report(true, "new_game", "actor spawned on level " .. level.name())
            return true
        end,
        function()
            local obj, dist = find_enemy()
            if not obj then
                report(false, "kill_enemy", "no living enemy found on the level")
                return true
            end
            target_id = obj:id()
            say(string.format("killing %s (id=%d, section=%s, %.1f m away)", obj:name(), target_id, obj:section(), dist))
            obj:kill(actor())
            write_state({ stage = "killed", target_id = target_id, target_name = obj:name() })
            return true
        end,
        wait(5000),
        function()
            if target_id then
                verify_dead(target_id, "kill_enemy")
            end
            return true
        end,
        function(elapsed)
            -- The save console command refuses to save while autosave isn't ready, so retry
            if save_file_exists() then
                report(true, "save", SAVE_NAME .. ".sav written")
                return true
            end
            if elapsed >= save_attempts * 10000 then
                if save_attempts == 3 then
                    report(false, "save", SAVE_NAME .. ".sav not written after 3 attempts")
                    console("quit")
                    steps = {}
                    return true
                end
                save_attempts = save_attempts + 1
                console("save " .. SAVE_NAME)
            end
            return false
        end,
        function()
            local s = read_state()
            s.stage = "saved"
            write_state(s)
            console("load " .. SAVE_NAME)
            return true
        end,
    }
end

local function load_steps()
    local state = read_state()
    return {
        wait(10000),
        function()
            report(true, "load_cold", "actor spawned from command line load of " .. SAVE_NAME)
            if state.target_id then
                verify_dead(tonumber(state.target_id), "enemy_dead_after_cold_load")
            else
                report(false, "enemy_dead_after_cold_load", "no target id recorded by the play phase")
            end
            return true
        end,
        wait(2000),
        function() console("quit") return true end,
    }
end

-- Let a dog pack run its group states (rest, idle animations) next to the actor.
-- The AI ignores the actor and god mode is on, so the pack doesn't switch to combat.
local function dogs_steps()
    return {
        wait(5000),
        function()
            console("ai_ignore_actor 1")
            console("g_god 1")
            local a = actor()
            local best, best_dist
            for id = 1, 65534 do
                local obj = level.object_by_id(id)
                if obj and obj:is_monster() and obj:alive() and string.find(obj:section(), "dog") then
                    local dist = obj:position():distance_to(a:position())
                    if not best or dist < best_dist then
                        best, best_dist = obj, dist
                    end
                end
            end
            if not best then
                report(false, "dogs_found", "no living dog online")
                console("quit")
                steps = {}
                return true
            end
            local pos = best:position()
            a:set_actor_position(vector():set(pos.x, pos.y + 5, pos.z))
            report(true, "dogs_found", string.format("%s (%s), %.1f m away, actor moved next to it", best:name(),
                best:section(), best_dist))
            return true
        end,
        wait(90000),
        function()
            report(true, "dogs_observed", "90 s next to the pack without crash")
            return true
        end,
        function() console("quit") return true end,
    }
end

-- Open every PDA tab and take a screenshot of each
local function pda_steps()
    local pda
    local function show_tab(section)
        return function()
            pda = pda or ActorMenu.get_pda_menu()
            if not pda:IsShown() then
                pda:ShowDialog(true)
            end
            pda:SetActiveSubdialog(section)
            local active = pda:GetActiveSection()
            report(active == section, "pda_tab_" .. section, "active section: " .. tostring(active))
            return true
        end
    end
    local function shot() console("screenshot") return true end
    -- SoC news icon: texture file plus rect (x, y, width, height), shown on the HUD
    local function news()
        db.actor:give_game_news("smoke test news", "ui\\ui_iconsTotal", Frect():set(0, 658, 83, 47), 0, 10000)
        return true
    end
    -- First story task with map locations (talk to Wolf), as given after the talk with Sidorovich
    local function task()
        db.actor:give_info_portion("tutorial_wounded_start")
        return true
    end
    -- All map locations to the engine log (debug builds), to check the map spots
    local function dump_map() console("dump_map") return true end
    -- The objective spots must sit on their story objects, not on the actor
    local function spots()
        local trader = alife():story_object(story_ids.Escape_Trader)
        local wolf = alife():story_object(story_ids.Escape_novice_lager_volk)
        local ok = trader and wolf and level.map_has_object_spot(trader.id, "blue_location") ~= 0 and
            level.map_has_object_spot(wolf.id, "green_location") ~= 0 and
            level.map_has_object_spot(db.actor:id(), "blue_location") == 0 and
            level.map_has_object_spot(db.actor:id(), "green_location") == 0
        report(ok, "pda_task_spots", string.format("trader %s, wolf %s, actor %d",
            trader and tostring(trader.id) or "?", wolf and tostring(wolf.id) or "?", db.actor:id()))
        return true
    end
    local result = { wait(8000), task, news, wait(1500), shot, dump_map, spots, wait(1000) }
    for _, section in ipairs({ "eptTasks", "eptMap", "eptLogs", "eptContacts", "eptRanking",
                               "eptStatistics", "eptEncyclopedia" }) do
        table.insert(result, show_tab(section))
        table.insert(result, wait(3000))
        table.insert(result, shot)
        table.insert(result, wait(2000))
    end
    table.insert(result, function() pda:HideDialog() console("quit") return true end)
    return result
end

-- Run the game time fast across the 6:00 boundary of the day/night music tracks and log the
-- game clock; the engine logs every started track ("- Play music track:", debug builds).
-- Music and effects are muted (user.ltx is restored by the runner).
local function music_steps()
    local result = {
        wait(3000),
        function()
            console("snd_volume_music 0")
            console("snd_volume_eff 0")
            level.set_time_factor(100)
            return true
        end,
    }
    for _ = 1, 40 do
        table.insert(result, wait(10000))
        table.insert(result, function()
            say(string.format("game clock %02d:%02d", level.get_time_hours(), level.get_time_minutes()))
            return true
        end)
    end
    table.insert(result, function()
        report(true, "music_observed", string.format("until %02d:%02d game time", level.get_time_hours(),
            level.get_time_minutes()))
        console("quit")
        return true
    end)
    return result
end

-- Task spots far from the actor: give the first story task, move the actor 575 m east of the trader
-- (as in a user's save), open the PDA map. The spots must stay on the level map texture
-- (a wrong bound_rect of the current level put them next to the actor).
local function farspots_steps()
    local pda
    return {
        wait(8000),
        function()
            console("g_god 1")
            console("ai_ignore_actor 1")
            db.actor:give_info_portion("tutorial_wounded_start")
            local vid = level.vertex_id(vector():set(317, 0, -8))
            db.actor:set_actor_position(level.vertex_position(vid))
            return true
        end,
        wait(15000),
        function()
            local trader = alife():story_object(story_ids.Escape_Trader)
            say("farspots trader online: " .. (trader and tostring(trader.online) or "?"))
            pda = ActorMenu.get_pda_menu()
            pda:ShowDialog(true)
            pda:SetActiveSubdialog("eptMap")
            return true
        end,
        wait(3000),
        function() console("screenshot") return true end,
        wait(2000),
        function() pda:HideDialog() console("quit") return true end,
    }
end

-- Loaded save: where are the actor, the trader, Wolf and their task spots (debug aid)
local function probe_steps()
    local function pos(se)
        return se and string.format("%d (%.0f, %.0f)", se.id, se.position.x, se.position.z) or "?"
    end
    return {
        wait(3000),
        function()
            local trader = alife():story_object(story_ids.Escape_Trader)
            local wolf = alife():story_object(story_ids.Escape_novice_lager_volk)
            local a = db.actor
            say(string.format("probe actor %d (%.0f, %.0f), trader %s, wolf %s", a:id(), a:position().x,
                a:position().z, pos(trader), pos(wolf)))
            for _, t in ipairs({ "blue_location", "green_location" }) do
                say(string.format("probe %s: actor %d trader %d wolf %d", t, level.map_has_object_spot(a:id(), t),
                    trader and level.map_has_object_spot(trader.id, t) or -1,
                    wolf and level.map_has_object_spot(wolf.id, t) or -1))
            end
            console("dump_map")
            console("quit")
            return true
        end,
    }
end

local function on_actor_bound()
    if phase == "play" then
        steps = play_steps()
    elseif phase == "load" then
        steps = load_steps()
    elseif phase == "dogs" then
        steps = dogs_steps()
    elseif phase == "pda" then
        steps = pda_steps()
    elseif phase == "music" then
        steps = music_steps()
    elseif phase == "farspots" then
        steps = farspots_steps()
    elseif phase == "probe" then
        steps = probe_steps()
    else
        return
    end
    step_index, step_started = 1, nil
    level.add_call(run_steps, function() end)
end

-- The actor binder differs per game (SoC: bind_stalker.actor_init, CS/CoP: bind_actor.init),
-- so take it from system.ltx like the engine does.
local function hook_actor_binder()
    local binding = system_ini():r_string("actor", "script_binding")
    local module_name, func_name = binding:match("^([%w_]+)%.([%w_]+)$")
    if not module_name then
        report(false, "hook", "unexpected [actor] script_binding: " .. tostring(binding))
        return
    end
    prefetch(module_name)
    local module = _G[module_name]
    local original = module and module[func_name]
    if type(original) ~= "function" then
        report(false, "hook", binding .. " not found")
        return
    end
    module[func_name] = function(obj)
        original(obj)
        say("actor bound")
        on_actor_bound()
    end
end

local function hook_main_menu()
    prefetch("ui_main_menu")
    local cls = ui_main_menu and ui_main_menu.main_menu
    if not cls then
        report(false, "hook", "ui_main_menu.main_menu not found")
        return
    end
    local original = cls.Update
    local menu_shown = false
    steps = {
        function()
            report(true, "main_menu", "main menu is updating")
            return true
        end,
        wait(3000),
        function() console("quit") return true end,
    }
    cls.Update = function(self)
        if original then
            original(self)
        else
            CUIScriptWnd.Update(self)
        end
        if not menu_shown then
            menu_shown = true
            say("main menu shown")
        end
        run_steps()
    end
end

say("driver loaded, phase=" .. phase)
if phase == "menu" then
    -- The main menu runs in the script engine created at application start;
    -- ALife never starts in this phase.
    hook_main_menu()
else
    hook_actor_binder()
end
