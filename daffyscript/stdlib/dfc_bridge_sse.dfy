module dfc_bridge_sse
version 1.0.0

-- Standard library reference module for the SSE-based frontend/backend bridge.
--
-- This file documents the intended shape of the newer bridge contract:
-- - backend extensions emit named events onto the room/frontend bridge
-- - the backend exposes those events as Server-Sent Events on `/bridge`
-- - the frontend subscribes with `EventSource`
-- - bridge consumers react through named hooks rather than raw DOM access
--
-- This module is intentionally tiny because the current Daffyscript toolchain does not
-- yet fully lower standard-library wrappers. It still serves as the canonical place to
-- document the payload shape and naming convention expected by the runtime.

struct BridgeEvent {
    name: str,
    payload_json: str,
}

pub fn emit_sse(name: str, payload_json: str) {
    -- Runtime expectation:
    -- call the imported `dfc.bridge.sse_emit` host function so the backend can publish
    -- a JSON event envelope onto the `/bridge` SSE stream.
    let outbound = payload_json
}

pub fn subscribe(name: str, handler_token: str) {
    -- Runtime expectation:
    -- register a named frontend hook subscription through `dfc.bridge.sse_subscribe`.
    let subscription = handler_token
}

exports {
    emit_sse,
    subscribe,
}
