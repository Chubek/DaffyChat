# DaffyChat Architecture Overview

## Tier 0 Decisions

This note locks the bootstrap architecture so implementation can begin without re-litigating scope on every subsystem.

### Client Scope

DaffyChat ships a hybrid MVP:

- Native clients own real-time voice transport.
- The bundled web frontend owns room entry, room state, text surfaces, extension hooks, and admin-visible diagnostics.
- Browser voice transport is explicitly out of scope for the MVP.

This reconciles the two top-level specs:

- `AGENTS.md` defines a native-to-native WebRTC voice product and excludes browser clients for voice.
- `README.md` defines a decoupled frontend and server-driven extension bridge.

The practical outcome is that the frontend is part of the product, but not part of the voice media plane.

### Runtime Isolation

The MVP starts with a process-backed room runtime behind an isolation interface.

- Tier 0 and Tier 1 use local processes for faster iteration and simpler tests.
- LXC remains the target hardened runtime for room isolation.
- Any room runtime entry point must be written against an abstraction so the implementation can switch from `process` to `lxc` without changing room orchestration APIs.

### Build Strategy

All three declared build specs are bootstrapped now:

- `CMakeLists.txt` is the most complete bootstrap entrypoint.
- `meson.build` and `build.meson` mirror the same source layout and options.
- `build.ninja` is a direct low-friction bootstrap for local smoke builds.

Packaging knobs for `.deb`, `.rpm`, and `.tar.gz` are exposed now, with full packaging flow to be completed in a later tier.

## System Shape

### Control Plane

The backend is responsible for:

- room registry and lifecycle
- participant/session tracking
- signaling coordination
- service metadata and room-scoped runtime orchestration
- frontend bridge event emission

### Media Plane

The native voice stack follows `AGENTS.md`:

- `uWebSockets` signaling server for `join`, `leave`, `offer`, `answer`, and `ice-candidate`
- `libdatachannel` for peer connection, ICE, DTLS, and RTP transport
- DTLS-derived SRTP keys for `libsrtp`
- `PortAudio` + `rnnoise` + `libsamplerate` + `Opus` for capture and playback

The backend and signaling binaries created in Tier 0 are placeholders only; no real media path exists yet.

### Extension Plane

The extension surface is designed in from the start even though the implementation is staged:

- DSSL defines microservice APIs and generated service skeletons.
- Daffyscript compiles to WASM for server-hosted extension logic.
- The frontend consumes server-emitted bridge events rather than loading arbitrary browser plugins directly.

### Presentation Plane

The bundled frontend is intentionally narrow during bootstrap:

- landing page
- room shell
- extension bridge bootstrap object
- diagnostics-friendly placeholders for room state and native voice handoff

## Repository Bootstrap Layout

The following directories are now considered stable foundations for future tiers:

- `include/daffy/` for public C++ headers
- `src/` for backend, signaling, runtime, and CLI sources
- `tests/` for unit, integration, e2e, fixtures, and golden assets
- `docs/architecture/`, `docs/api/`, and `docs/operations/`
- `config/` for sample and future deployable configuration
- `frontend/app/` for the bundled web UI state, components, hooks, and styles

## Configuration Surface

Tier 0 standardizes a JSON configuration document with these sections:

- `server`
- `signaling`
- `turn`
- `runtime_isolation`
- `services`
- `frontend_bridge`

JSON is used for bootstrap because it is easy to inspect, diff, template, and validate. A typed loader is a Tier 1 task.

## Constraints Locked For Later Tiers

- Voice remains peer-to-peer and room size stays capped for the first media slice.
- Browser-based media is not part of the MVP.
- PortAudio callbacks must remain allocation-free and non-blocking.
- `rnnoise` must receive 480-sample mono float32 frames at 48 kHz.
- SRTP material must come from the DTLS context, never from an independent key path.
- Services communicate over `nng` IPC.
- Packaging outputs remain required deliverables: `.deb`, `.rpm`, `.tar.gz`.

## Tier 0 Exit State

Tier 0 is complete when:

- the repo configures with CMake, Meson, and Ninja
- the backend, signaling, and bootstrap test binaries compile
- the sample config captures the agreed runtime surface
- the frontend has a bootstrap bridge shell that matches the architecture
