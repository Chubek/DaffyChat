module message_heatmap
version 1.0.0

import dfc_bridge_sse

-- This module is a frontend-oriented bridge plugin.
--
-- Why it is useful:
-- - long rooms accumulate dense message threads
-- - moderators and active participants need a quick way to spot where the room is "hot"
-- - the browser should react to server-side or room-side events without becoming a browser voice client
--
-- How the extensibility model works here:
-- 1. server-side extension logic decides what happened
-- 2. it emits bridge events, not arbitrary DOM mutations
-- 3. the frontend chooses how to honor those events through named hooks
-- 4. this keeps the browser decoupled while still enabling rich UI behavior

struct HeatmapPoint {
    message_id: str,
    intensity: int,
    reason: str,
}

pub fn pulse(point: HeatmapPoint) {
    -- The payload shape is intentionally explicit so a frontend bridge consumer can
    -- validate or transform it before touching UI state.
    emit "stdext.message-heatmap.pulse" {
        message_id: point.message_id,
        intensity: point.intensity,
        reason: point.reason,
    }
}

on hook "room:participant-joined" (payload: {peerId: str}) {
    -- A real implementation could consult storage or metrics before deciding to pulse.
    -- In this demonstrative sample we simply announce that a new social focal point exists.
    emit "stdext.message-heatmap.hint" {
        peer_id: payload.peerId,
        message: "A new participant joined; recent welcome messages may deserve highlighting."
    }
}

expect hook "stdext.message-heatmap.pulse"
expect hook "stdext.message-heatmap.hint"

exports {
    pulse,
    on hook "room:participant-joined",
}
