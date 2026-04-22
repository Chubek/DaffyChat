# DaffyChat Extensibility

DaffyChat is designed around multiple extension surfaces. This document explains their intended roles and current maturity.

## Extension Surfaces

### Daffy Services (DSSL)

DSSL describes structured service APIs including types, RPCs, events, REST surfaces, shell hooks, and metadata. The repository already contains:

- the DSSL language reference in `dssl/README.md`
- bootstrap tooling such as `toolchain/dssl-init.py`
- native tools including `dssl-bindgen`, `dssl-docstrip`, and `dssl-docgen`

Intended deployment model:

- each service specification generates a daemon
- daemons communicate over `nng`
- `daffydmd` supervises lifecycle and metadata

### Lua Scripts

Lua remains the planned server-side lightweight scripting surface. The repository vendors Lua, but the DaffyChat integration layer is not complete yet.

### Daffyscript

Daffyscript is the planned room recipe and extension language that compiles into DaffyChat-specific WASM. The bootstrap compiler target exists, but the end-to-end runtime is still incomplete.

### Shared Library Plugins

Native plugins remain a first-class extension model for operators who want tightly integrated server-side modules. The plugin API still needs stronger documentation, examples, and Doxygen-backed reference material.

### REST APIs And Event Bus

Each room exposes server-managed APIs and emits room-scoped events. The bundled frontend is expected to consume these via bridge endpoints and `EventSource`-style event delivery.

## Choosing An Extension Strategy

- Choose DSSL when you need a structured service boundary and code generation.
- Choose Daffyscript when you want portable room recipes and future WASM deployment.
- Choose shared libraries when you need native performance and tight process integration.
- Choose REST and event hooks when you want thin integrations from external systems.

## What Is Missing

- concrete example services in `service/`
- a documented plugin ABI with generated API docs
- more example extensions in `stdext/`
- a linter and richer toolchain coverage for Daffyscript

## Near-Term Recommended Direction

The next practical extension milestone is to ship one end-to-end DSSL example service plus generated metadata, packaging, and tests. That would validate the intended daemon-manager model without requiring the entire extension matrix at once.
