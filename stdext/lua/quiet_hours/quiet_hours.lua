-- quiet_hours.lua
--
-- This Lua extension demonstrates the lightweight scripting surface described in the
-- project README. The exact injected globals may evolve, so the script is written as
-- a reference extension that focuses on lifecycle, event flow, and failure-safe habits.
--
-- What it is useful for:
-- - reducing noisy automation in off-hours rooms
-- - showing how a room script can intercept commands and make policy decisions
-- - documenting the expected relationship between Lua, the room event bus, and services
--
-- Assumed host libraries from the DaffyChat architecture notes:
-- - `ldcevent`   for event bus publish/subscribe
-- - `ldcmessage` for room messaging / command interception
-- - `ldcservice` for calling generated DSSL-backed services
--
-- The function and object names below are intentionally explicit rather than clever.

local quiet_hours = {
  start_hour_utc = 22,
  end_hour_utc = 7,
  suppress_commands = {
    ["/poll"] = true,
    ["/standup-start"] = true,
  },
}

-- Returns true when the current UTC hour falls inside the quiet-hours policy.
local function in_quiet_window(now)
  local hour = tonumber(os.date("!%H", now or os.time()))
  if quiet_hours.start_hour_utc < quiet_hours.end_hour_utc then
    return hour >= quiet_hours.start_hour_utc and hour < quiet_hours.end_hour_utc
  end
  return hour >= quiet_hours.start_hour_utc or hour < quiet_hours.end_hour_utc
end

-- Intercept slash commands before other automation becomes noisy.
local function on_command(payload)
  if not payload or not payload.command then
    return
  end

  if not in_quiet_window() then
    return
  end

  if quiet_hours.suppress_commands[payload.command] then
    ldcmessage.send("Quiet hours are active. Please retry this command during the daytime handoff window.")
    ldcevent.emit("stdext.quiet_hours.command_suppressed", {
      command = payload.command,
      actor = payload.actor or "unknown",
    })
    return false
  end
end

-- A room script should announce its own policy state so observability surfaces can render it.
local function on_room_ready()
  ldcevent.emit("stdext.quiet_hours.loaded", {
    start_hour_utc = quiet_hours.start_hour_utc,
    end_hour_utc = quiet_hours.end_hour_utc,
  })
end

-- The subscription names are illustrative. They mirror the room-local event bus concept
-- described in the repo docs and serve as a concrete example for future runtime wiring.
ldcevent.on("room.ready", on_room_ready)
ldcmessage.on_command(on_command)
