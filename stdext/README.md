# DaffyChat Standard Extensions (`stdext`)

This directory is a deliberately over-documented pack of sample extensions for the
current DaffyChat tree.

The goal is twofold:

1. give future implementers a concrete starting point for each extension surface
2. show, in one place, how the room/service/frontend extensibility model is intended to fit together

Important reality check:

- DSSL is the most concrete extension surface in-tree today. These `.dssl` files are meant to parse cleanly and drive `dssl-bindgen` / `dssl-docstrip` / `dssl-docgen`.
- Daffyscript exists in-tree, but its compiler is still shallow. The `.dfy`, `.dfyp`, and `.dfyr` examples here are intentionally written as design-forward examples that mirror the README and current examples. Treat them as heavily commented reference plugins, not as a claim that the full runtime wiring is complete.
- Lua room scripting and shared-library plugin loading are architecturally part of DaffyChat, but room/runtime integration is still staged. The Lua and shared-library examples in this directory therefore focus on API shape, lifecycle expectations, and extension contracts.

What is included:

- `dssl/room_analytics`: room analytics and admin-facing metrics service contract
- `dssl/moderation_assistant`: moderation workflow contract for flags, queueing, and actions
- `dssl/voice_ops`: voice diagnostics and troubleshooting service contract
- `daffyscript/frontend/message_heatmap`: frontend bridge module that highlights active threads
- `daffyscript/frontend/participant_status`: frontend bridge module that decorates participant presence/voice state
- `daffyscript/programs/standup_helper`: room bot that helps teams run async standups
- `daffyscript/recipes/incident_bridge`: recipe that assembles diagnostics, moderation, and automation into one room profile
- `lua/quiet_hours`: a room script that suppresses noisy bot behavior during configured quiet windows
- `lua/session_journal`: a room script that turns room events into a lightweight session journal
- `shared/dssl_markdown_target`: a shared-library example implementing the public DSSL plugin API

How these examples map to the architecture:

- DSSL defines contracts and generated skeletons for services.
- Daffyscript modules define frontend bridge behavior and server-hosted WASM hooks.
- Daffyscript programs define room bot / automation behavior.
- Daffyscript recipes define how multiple services, programs, modules, roles, and webhooks get assembled into a room profile.
- Lua scripts are the low-friction room-local automation layer.
- Shared libraries extend the tooling itself, such as `dssl-bindgen` targets.

If you are implementing the runtime behind these examples, start by reading:

- `README.md`
- `docs/architecture/overview.md`
- `dssl/README.md`
- `daffyscript/README.md`
