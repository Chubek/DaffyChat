module participant_status_overlay
version 1.0.0

import dfc.bridge

-- This module demonstrates a second frontend bridge plugin with a different job:
-- it decorates participant rows with state that originates from signaling and native voice telemetry.
--
-- The key lesson is that Daffyscript modules do not need direct browser media access.
-- They only need a stable event contract and a frontend shell that knows how to render it.

pub fn set_badge(peer_id: str, label: str, tone: str) {
    emit "stdext.participant-status.badge" {
        peer_id: peer_id,
        label: label,
        tone: tone,
    }
}

on hook "voice:state-changed" (payload: {transport: str}) {
    -- In a full runtime this hook would likely receive richer payload data.
    -- We still demonstrate a valuable pattern: translate low-level telemetry into UI-friendly events.
    emit "stdext.participant-status.voice-banner" {
        transport: payload.transport,
        headline: "Native voice state changed",
    }
}

expect hook "stdext.participant-status.badge"
expect hook "stdext.participant-status.voice-banner"

exports {
    set_badge,
    on hook "voice:state-changed",
}
