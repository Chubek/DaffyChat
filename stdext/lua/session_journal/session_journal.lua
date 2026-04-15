-- session_journal.lua
--
-- This extension demonstrates a second Lua script with a different job: keep a small,
-- room-local journal of meaningful events that later services or bots can summarize.
--
-- Why it is useful:
-- - incident rooms often need a lightweight chronology
-- - study rooms may want a compact summary of join/leave/publish events
-- - it shows how Lua can sit between event bus activity and storage-backed summaries

local JOURNAL_KEY = "stdext.session_journal.entries"
local MAX_ENTRIES = 50

local function load_entries()
  return ldcstorage.get(JOURNAL_KEY) or {}
end

local function save_entries(entries)
  ldcstorage.set(JOURNAL_KEY, entries)
end

local function append_entry(kind, payload)
  local entries = load_entries()
  table.insert(entries, {
    kind = kind,
    at = os.date("!%Y-%m-%dT%H:%M:%SZ"),
    payload = payload,
  })

  while #entries > MAX_ENTRIES do
    table.remove(entries, 1)
  end

  save_entries(entries)
end

local function on_join(payload)
  append_entry("participant.join", payload)
end

local function on_leave(payload)
  append_entry("participant.leave", payload)
end

local function on_voice_state(payload)
  append_entry("voice.state", payload)
end

local function publish_summary()
  local entries = load_entries()
  ldcmessage.send("Session journal currently has " .. tostring(#entries) .. " recorded events.")
end

ldcevent.on("room:participant-joined", on_join)
ldcevent.on("room:participant-left", on_leave)
ldcevent.on("voice:state-changed", on_voice_state)
ldcmessage.on_command(function(payload)
  if payload and payload.command == "/journal" then
    publish_summary()
  end
end)
