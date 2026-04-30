# Voice Transport API

This document defines the transport-agnostic signaling API used to drive DaffyChat voice sessions from frontend or external clients (browser, Android, desktop, bots).

The API lives on the signaling server (same host/port as signaling WebSocket).

## Goal

- Decouple voice signaling from a specific client runtime.
- Let any client communicate with DaffyChat using a stable HTTP API.
- Reuse the same backend signaling core used by native/WebSocket clients.

## Base URL

- `http://<signaling-host>:<signaling-port>`
- Example: `http://127.0.0.1:7001`

## Endpoints

### 1) Create session

- **Method:** `POST`
- **Path:** `/api/signaling/connect`
- **Body JSON:**
  - `peer_id` (string, required)
- **Response 200 JSON:**
  - `connection_id` (string): opaque session id to use in later calls
  - `peer_id` (string)
  - `transport` (string): currently `"http-api"`

Example request:

```json
{"peer_id":"peer-web-1"}
```

Example response:

```json
{"connection_id":"api-conn_abc123","peer_id":"peer-web-1","transport":"http-api"}
```

### 2) Send signaling message

- **Method:** `POST`
- **Path:** `/api/signaling/send`
- **Body JSON:**
  - `connection_id` (string, required)
  - `message` (string, required): serialized signaling JSON message understood by backend signaling core
- **Response 202 JSON:**
  - `status`: `"queued"`

Example:

```json
{
  "connection_id":"api-conn_abc123",
  "message":"{\"type\":\"join\",\"room\":\"room-1\",\"peer_id\":\"peer-web-1\"}"
}
```

### 3) Poll outbound events

- **Method:** `GET`
- **Path:** `/api/signaling/events?connection_id=<id>`
- **Response 200 JSON:**
  - `events` (array of strings): each item is a serialized signaling JSON message destined for the client

Example response:

```json
{
  "events":[
    "{\"type\":\"join-ack\",\"room\":\"room-1\",\"peer_id\":\"peer-web-1\"}",
    "{\"type\":\"peer-ready\",\"room\":\"room-1\",\"peer_id\":\"peer-native-2\"}"
  ]
}
```

## Signaling message contract

`message` passed to `/api/signaling/send` is the same JSON used by WebSocket signaling:

- `join`, `leave`
- `offer`, `answer`
- `ice-candidate`
- plus existing signaling server message types

This keeps one backend state machine for all transports.

## Error handling

- `400 Bad Request`: malformed or missing fields.
- `404 Not Found`: unknown `connection_id` in events polling.
- `202 Accepted`: send accepted and processed; check `/api/signaling/events` for resulting outbound messages.

## Transport model

- The API is a long-lived logical connection keyed by `connection_id`.
- Outbound messages are buffered server-side per connection.
- Client pulls buffered events via polling.

## Security and production hardening roadmap

Recommended expansions for production:

1. Add auth on all `/api/signaling/*` endpoints (JWT/session token).
2. Bind `connection_id` to authenticated principal and room permissions.
3. Add idle timeout and explicit disconnect endpoint.
4. Add queue size limits and backpressure policy.
5. Add message schema validation before `HandleMessage`.
6. Add rate limiting per IP/principal.
7. Add trace ids and structured audit logs.

## Expansion options

### A) SSE transport for events

- Keep `connect/send` as-is.
- Add `/api/signaling/events/stream` (Server-Sent Events) to avoid polling.
- Preserve same backend queue and message format.

### B) WebSocket transport facade

- Keep API message schema unchanged.
- Wrap schema in WS frames for lower latency.
- Reuse same session and signaling core.

### C) Socket.IO facade

- Map `signal` event payloads to the same message schema.
- Keep backend core untouched; implement only adapter layer.

### D) DSSL control plane integration

- Define DSSL verbs for session open/send/poll:
  - `voice.connect`
  - `voice.send`
  - `voice.events`
- DSSL executor can call these endpoints internally.

## Minimal client flow

1. `POST /api/signaling/connect`
2. `POST /api/signaling/send` with `join`
3. poll `/api/signaling/events` for `join-ack` and peer events
4. exchange `offer/answer/ice-candidate` through `send` + `events`

This is sufficient to support browser and mobile frontends now, while preserving forward compatibility with SSE/WebSocket/Socket.IO facades later.
