# DaffyChat Guide

This guide is the shortest path to understanding the current repository shape.

## What DaffyChat Is

DaffyChat is a native-first, room-based chat platform with three major planes:

- a backend control plane for rooms, signaling orchestration, and extension metadata
- a native voice plane built around WebRTC-compatible transport pieces
- an extension plane spanning DSSL services, Lua, shared-library plugins, REST hooks, and future Daffyscript recipes

The current bootstrap implementation focuses on architecture, buildability, test scaffolding, and interfaces rather than a complete end-user feature set.

## Main Components

- `src/cli/` contains entry points for backend and signaling processes.
- `src/signaling/` contains signaling protocol and HTTP/WebSocket server scaffolding.
- `src/voice/` contains the native voice stack scaffolding.
- `src/services/` and `src/ipc/` contain service registry and NNG transport foundations.
- `dssl/` contains the Daffy Service Specification Language tools and documentation.
- `daffyscript/` contains the bootstrap compiler entry point for room recipes.
- `frontend/` contains the bundled web assets and bridge shell.

## Configuration Model

The sample JSON configuration defines these top-level areas:

- `server`
- `signaling`
- `turn`
- `runtime_isolation`
- `services`
- `frontend_bridge`
- `voice`

Treat `config/daffychat.example.json` as the canonical bootstrap reference.

## Typical Developer Workflow

1. Configure with CMake.
2. Build the requested binaries or tests.
3. Run targeted CTest cases.
4. Edit the sample configuration for local experiments.
5. Use the tooling in `toolchain/` to prototype DSSL and recipe workflows.

## Docs Map

- `INSTALL.md` explains local build and installation.
- `DEPLOY.md` explains packaging and service rollout.
- `EXTENSIBILITY.md` explains extension surfaces.
- `RECIPES.md` explains room recipes and Daffyscript’s current place in the design.
- `docs/architecture/overview.md` explains the locked architectural decisions.
