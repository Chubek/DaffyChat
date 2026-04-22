# DaffyChat

DaffyChat is a native-first, extensible chatroom platform with a bundled web frontend, a native voice stack, and multiple planned extension surfaces including DSSL services, Daffyscript recipes, Lua, REST APIs, and shared-library plugins.

## Repository Status

The current tree is a bootstrap implementation. Core architecture, build targets, packaging helpers, and test scaffolding are present, while several runtime-heavy subsystems are still incomplete.

## Start Here

- `INSTALL.md` for local build and installation
- `GUIDE.md` for a quick orientation to the codebase
- `DEPLOY.md` for deployment and packaging notes
- `EXTENSIBILITY.md` for extension surfaces and roadmap
- `RECIPES.md` for room recipe concepts and current status
- `docs/architecture/overview.md` for the locked bootstrap architecture
- `dssl/README.md` for the DSSL language reference

## Current Primary Targets

- `daffy-backend`
- `daffy-signaling`
- `dssl-bindgen`
- `dssl-docstrip`
- `dssl-docgen`
- `daffyscript`

## Current Gaps

- `daffydmd` is not implemented yet
- default services under `service/` are not implemented yet
- extension examples and plugin API docs are still sparse
- deployment support is scaffolded but not production complete

## Build

```sh
cmake -S . -B build/cmake -G Ninja
cmake --build build/cmake
```
