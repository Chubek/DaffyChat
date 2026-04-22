# DaffyChat Bot API v1.0 Draft

## Purpose

The Bot API is the built-in service and REST surface that lets automated agents join rooms, observe room lifecycle/events, post messages, and perform controlled moderation or workflow actions.

This document defines the first production-target contract for the Bot API service that DaffyChat should ship by default in v1.0.

## Goals

- provide a stable built-in API for room bots;
- allow bots to authenticate independently from human operators;
- let bots consume room lifecycle and message/event streams;
- support message sending, command handling, and basic moderation flows;
- fit the existing `daffydmd` + service model and the room event bus;
- expose both a service-level IPC contract and a room-facing HTTP contract.

## Non-Goals

- arbitrary code execution through the Bot API;
- full plugin management;
- cross-room super-admin capabilities by default;
- permanent message history guarantees;
- a public unauthenticated bot surface.

## Architecture

The Bot API should exist in two layers:

1. a built-in managed service, tentatively named `botapi`, supervised by `daffydmd`;
2. a room-facing REST facade that maps HTTP requests to the service contract.

The managed service is the source of truth for:

- bot registration and token validation;
- room membership state for bots;
- bot capability checks;
- room event subscriptions for bots;
- command dispatch and response generation;
- rate limiting and audit logging.

## Transport Model

### Service IPC

The internal service runs over NNG request/reply using topic:

- `service.botapi`

Message type:

- `request`
- `reply`

### REST Surface

The default room REST facade should expose endpoints under:

- `/api/bot/v1`

All REST handlers should translate to the same internal RPC model used by the managed service.

## Authentication

### Bot Identity

Each bot has:

- `bot_id`
- `display_name`
- `token_id`
- `token_secret` or bearer token
- `capabilities`
- `enabled`
- optional `room_scope`

### Auth Modes

v1.0 should support:

- `Authorization: Bearer <token>` for REST;
- token field in service IPC payloads for managed/internal callers.

### Required Validation

Each authenticated call must validate:

- token exists;
- bot is enabled;
- requested room is in scope, if scoped;
- requested action is permitted by bot capabilities.

## Capabilities

Initial capability set:

- `rooms.read`
- `rooms.join`
- `events.read`
- `messages.write`
- `commands.handle`
- `participants.read`
- `participants.kick`
- `participants.mute`
- `webhooks.write`

Bots must be denied by default for actions outside their granted capabilities.

## Core Data Model

### BotRecord

- `bot_id: string`
- `display_name: string`
- `enabled: bool`
- `created_at: string`
- `updated_at: string`
- `capabilities: string[]`
- `room_scope: string[]`
- `metadata: object`

### BotSession

- `session_id: string`
- `bot_id: string`
- `room_id: string`
- `joined_at: string`
- `last_seen_at: string`
- `state: string` (`joining`, `active`, `suspended`, `left`)

### BotEventCursor

- `bot_id: string`
- `room_id: string`
- `last_sequence: number`
- `updated_at: string`

### BotCommand

- `command_id: string`
- `room_id: string`
- `bot_id: string`
- `name: string`
- `args: object`
- `issued_by: string`
- `issued_at: string`

## IPC RPC Contract

### `RegisterBot`

Creates or updates a bot registration.

Request:

```json
{
  "rpc": "RegisterBot",
  "display_name": "welcome-bot",
  "capabilities": ["rooms.join", "events.read", "messages.write"],
  "room_scope": ["room-alpha"],
  "metadata": {"kind": "welcome"}
}
```

Reply:

```json
{
  "bot": {
    "bot_id": "bot-123",
    "display_name": "welcome-bot",
    "enabled": true,
    "capabilities": ["rooms.join", "events.read", "messages.write"],
    "room_scope": ["room-alpha"]
  },
  "token": {
    "token_id": "token-123",
    "bearer": "dcb_..."
  }
}
```

### `GetBot`

Returns one bot record.

Request fields:

- `rpc`
- `bot_id`

### `ListBots`

Returns all bots visible to the caller.

Optional filters:

- `enabled`
- `room_id`
- `capability`

### `JoinRoom`

Creates a bot room session.

Request fields:

- `rpc`
- `token`
- `room_id`

Reply fields:

- `session`
- `participant`

This should create a room participant with role `bot`.

### `LeaveRoom`

Ends a bot room session.

Request fields:

- `rpc`
- `token`
- `room_id`

### `PostMessage`

Submits a bot-authored room message.

Request fields:

- `rpc`
- `token`
- `room_id`
- `text`
- optional `attachments`
- optional `metadata`

Reply fields:

- `message_id`
- `accepted`
- `emitted_event_sequence`

### `PollEvents`

Returns room events for a bot using an event cursor.

Request fields:

- `rpc`
- `token`
- `room_id`
- optional `after_sequence`
- optional `limit`

Reply fields:

- `events`
- `next_sequence`

This should align with the event replay model already used by the built-in event bridge.

### `HandleCommand`

Submits a structured bot command for execution.

Request fields:

- `rpc`
- `token`
- `room_id`
- `name`
- `args`
- optional `issued_by`

Reply fields:

- `command_id`
- `accepted`
- optional `result`

### `ModerateParticipant`

Executes a limited moderation action.

Request fields:

- `rpc`
- `token`
- `room_id`
- `participant_id`
- `action` (`kick` or `mute`)
- optional `reason`

Reply fields:

- `accepted`
- `action`
- `participant_id`

### `Status`

Returns bot service health and counts.

Reply fields:

- `service_name`
- `status`
- `registered_bots`
- `active_sessions`
- `tracked_rooms`

## REST Contract

### `POST /api/bot/v1/bots`

Registers a bot.

### `GET /api/bot/v1/bots/{bot_id}`

Returns bot metadata.

### `GET /api/bot/v1/bots`

Lists bots.

### `POST /api/bot/v1/rooms/{room_id}/join`

Joins a room as the authenticated bot.

### `POST /api/bot/v1/rooms/{room_id}/leave`

Leaves a room as the authenticated bot.

### `POST /api/bot/v1/rooms/{room_id}/messages`

Posts a bot-authored message.

Body:

```json
{
  "text": "hello from bot",
  "metadata": {"intent": "welcome"}
}
```

### `GET /api/bot/v1/rooms/{room_id}/events?after_sequence=12&limit=50`

Polls room events for the authenticated bot.

### `POST /api/bot/v1/rooms/{room_id}/commands`

Submits a bot command.

### `POST /api/bot/v1/rooms/{room_id}/participants/{participant_id}/moderation`

Applies a permitted moderation action.

## Event Model

The Bot API should consume and/or emit these event families:

- `room.lifecycle`
- `room.message.created`
- `room.command.requested`
- `room.command.completed`
- `room.participant.moderated`
- `bot.session.started`
- `bot.session.ended`
- `bot.message.posted`

At v1.0, `room.lifecycle` is mandatory. Message and command event families can be added behind the same replay contract once room messaging lands.

## Error Model

Standard error payload:

```json
{
  "error": {
    "code": "forbidden",
    "message": "Bot lacks capability messages.write",
    "details": {
      "capability": "messages.write"
    }
  }
}
```

Minimum error codes:

- `unauthenticated`
- `forbidden`
- `not_found`
- `invalid_argument`
- `rate_limited`
- `conflict`
- `unavailable`

## Rate Limiting

v1.0 should enforce per-bot limits for:

- join/leave churn;
- message posting;
- event polling;
- moderation actions.

Suggested starting defaults:

- joins: 10/minute
- messages: 30/minute
- polls: 120/minute
- moderation actions: 10/minute

## Auditing

The service should record at least:

- bot registration changes;
- auth failures;
- room joins/leaves;
- posted messages;
- moderation actions;
- command dispatches.

Audit records should be compatible with future LMDB-backed persistence.

## Security Requirements

- bearer tokens must never be logged in plaintext;
- room-scoped bots must not escape scope;
- moderation requires explicit capability grants;
- disabled bots must fail closed immediately;
- HTTP endpoints must reject missing or malformed auth headers;
- internal RPC calls must validate the same capability rules as REST.

## Integration With Existing Built-ins

The Bot API should build on the existing runtime pieces already added:

- `health` for service health checks;
- `roomstate` for room metadata and participant/session state;
- `eventbridge` for replayable room event consumption and webhook-style fanout.

The preferred implementation path is:

1. authenticate bot token;
2. use `roomstate`-compatible room operations for join/leave and participant modeling;
3. use `eventbridge`-compatible sequence polling for event consumption;
4. emit bot-specific events back onto the room/runtime event bus.

## Suggested v1.0 Implementation Order

1. implement built-in managed service `botapi` with RPCs:
   - `RegisterBot`
   - `JoinRoom`
   - `LeaveRoom`
   - `PollEvents`
   - `Status`
2. add token auth and capability checks;
3. add REST facade under `/api/bot/v1`;
4. add `PostMessage` once room message primitives land;
5. add `HandleCommand` and `ModerateParticipant` once command/moderation state is available.

## Minimum Acceptance Criteria

DaffyChat v1.0 Bot API is considered minimally complete when:

- a bot can be registered and issued a token;
- a bot can join a room as a `bot` participant;
- a bot can poll room events through a stable cursor/sequence model;
- capability checks are enforced;
- the Bot API is supervised by `daffydmd` as a built-in service;
- at least one automated integration test covers registration, join, and event polling.
