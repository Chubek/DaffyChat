# DaffyChat Granular Completion Plan

## Goal

Bring DaffyChat from its current bootstrap state to a release-ready system with:
- runnable room and service runtimes;
- a working daemon manager (`daffydmd`);
- real `nng`-backed service IPC;
- usable extension flows across DSSL, Daffyscript, Lua, REST, and shared-library plugins;
- complete install/deploy/package/documentation coverage;
- end-to-end validation for the supported developer and operator workflows.

## Current Snapshot

The repository already has meaningful scaffolding, but completion still requires major work in the runtime path:
- docs, install/deploy scaffolding, and archive packaging bootstrap exist;
- DSSL parsing/sema/codegen scaffolding exists;
- a first echo-service vertical slice exists;
- service registry and metadata layers exist;
- the room/runtime/event-bus/frontend surfaces exist in partial form;
- `daffydmd`, default services, container lifecycle, generated-service workflow, extension examples, and several operator-facing artifacts remain incomplete.

## Completion Definition

DaffyChat should only be treated as complete when all of the following are true:
1. A room can be created, started, stopped, restored, and destroyed through a supported control path.
2. At least one DSSL-defined service can be generated, built, launched, registered with `daffydmd`, and reached from a room or test harness over real IPC.
3. The frontend can load Daffyscript-generated WASM artifacts and receive room events through the intended bridge/event pipeline.
4. The documented install, deploy, package, and extension workflows all work on a clean machine.
5. The project ships with docs, manpages, examples, packaging targets, and automated tests that cover the supported workflows.

## Delivery Strategy

Work should proceed in the order below, because each phase unlocks the next one:
1. stabilize the service toolchain and daemon runtime;
2. implement the missing default runtime surfaces;
3. complete the extension and frontend loading flows;
4. finish packaging, docs, examples, and release validation;
5. only then add alternate build-system parity.

## Phase 0: Baseline And Scope Lock

### 0.1 Repository hygiene
- Audit the current worktree and separate bootstrap work from unfinished experiments.
- Decide which in-progress files become canonical and which stay as prototypes.
- Normalize naming mismatches from the project brief where needed, for example `RECIPES.md` vs `RECIPEs.md`, and confirm whether `READNE.md` is a typo for `README.md` or an intentional compatibility artifact.
- Ensure `AGENT_PROGRESS.md` remains the source of truth for implementation history.

### 0.2 Supported-platform matrix
- Pick the first-class development targets, at minimum one Linux distro family for package output and one compiler family.
- Define which optional dependencies are required, vendored, or feature-gated.
- Document the minimum supported runtime assumptions for `systemd`, `lxc`, `nng`, LMDB, and the toolchain.

### 0.3 Definition-of-done checklist
- Turn the completion definition into a tracked checklist in the repo.
- Mark which items are blocker-level, release-level, or stretch goals.
- Freeze a first release scope so work does not drift.

Exit criteria:
- the team has a fixed release scope, a supported-platform matrix, and a stable naming/documentation baseline.

## Phase 1: DSSL Toolchain Completion

### 1.1 Parser and semantic coverage
- Expand parser tests for structs, enums, unions, RPCs, REST declarations, `exec`, constants, metadata, imports, nested types, invalid syntax, and docstrings.
- Add semantic checks for duplicate names, unknown types, unsupported recursive references, invalid return signatures, and malformed imports.
- Add golden-input and golden-output fixtures under `tests/` for representative service specs.

### 1.2 C++ code generation
- Finish `dssl/codegen/cpp/cpp_gen.hpp` so it emits compilable headers, JSON helpers, skeleton implementations, and service adapters for supported RPC patterns.
- Make generator output deterministic so checked-in fixtures and packaging are stable.
- Support multiple RPCs per service instead of a single happy-path adapter.
- Add explicit generator diagnostics for unsupported constructs instead of silently emitting unusable code.

### 1.3 `dssl-bindgen` CLI workflow
- Make `dssl-bindgen` the canonical entrypoint for generating service artifacts.
- Add flags for output layout, namespace override, validation-only mode, and fixture generation if needed.
- Ensure generated outputs can be produced into `services/generated/` from a clean checkout.
- Document the command sequence for developers adding a new DSSL service.

### 1.4 Build integration for generated services
- Replace purely checked-in exemplar use with a repeatable build path that can regenerate or verify generated artifacts.
- Decide whether generation occurs at configure time, build time, or via an explicit target.
- Add CMake targets for generating service code from DSSL specs.
- Fail the build when generated sources are stale or missing.

### 1.5 Toolchain validation
- Add focused tests that parse a real spec, generate code, compile it, and exercise at least one RPC adapter.
- Add CLI smoke tests for `dssl-bindgen`, `dssl-docgen`, and `dssl-docstrip`.

Exit criteria:
- a DSSL file can be validated, code-generated, compiled, and invoked through an automated build/test path.

## Phase 2: IPC Layer And Transport Hardening

### 2.1 Request/reply transport
- Finalize `include/daffy/ipc/nng_transport.hpp` and `src/ipc/nng_transport.cpp` around real `nng` request/reply sockets.
- Remove any remaining purely in-memory semantics hidden behind the current class names.
- Support explicit URL validation, startup readiness, timeout configuration, and clean shutdown.
- Ensure IPC socket cleanup works for `ipc://` endpoints across repeated test runs.

### 2.2 Pub/sub transport
- Complete real `nng` pub/sub delivery semantics.
- Handle subscription setup races so early publishes are not silently dropped in tests.
- Add predictable shutdown behavior for worker threads and bound sockets.

### 2.3 Error model and observability
- Normalize transport errors to `daffy::core::Error` with actionable messages.
- Add structured logging hooks around bind, dial, request, reply, publish, and shutdown paths.
- Expose enough diagnostics for daemon-manager debugging.

### 2.4 Transport tests
- Add integration tests for request/reply, publish/subscribe, malformed payloads, duplicate binds, timeouts, and reconnect behavior.
- Run IPC-backed tests under realistic endpoint paths in `/tmp`.

Exit criteria:
- service-to-service and daemon-to-service messaging work reliably over real `nng` sockets in automated tests.

## Phase 3: `daffydmd` Minimum Viable Daemon Manager

### 3.1 Core daemon-manager library
- Finish the `include/daffy/runtime/daemon_manager.hpp` and `src/runtime/daemon_manager.cpp` slice into a coherent library surface.
- Model managed service state with metadata, pid, endpoint URL, health/status, and timestamps.
- Define control-plane messages for register, unregister, list, health, reload, and proxy/broker requests.

### 3.2 Process supervision
- Implement process launch and stop flows for generated services.
- Track child PIDs and restart policy decisions.
- Write and reconcile pid files in the intended runtime directory.
- Gracefully handle stale pid files and crashed child processes.

### 3.3 Persistent state
- Introduce LMDB-backed state storage for service records, pid snapshots, and restart history.
- Keep in-memory state and persisted state consistent on startup and shutdown.
- Add recovery behavior for daemon restart.

### 3.4 Broker behavior
- Route request/reply traffic from rooms or control clients to managed services.
- Add routing for service discovery and health checks.
- Define the boundary between direct room-to-service IPC and `daffydmd`-brokered IPC.

### 3.5 CLI and systemd surface
- Expand `src/cli/daffydmd_main.cpp` from bootstrap output into a real daemon entrypoint.
- Support config-path arguments, foreground mode, logging level, pid-file location, and reload semantics.
- Align runtime behavior with `scripts/daffydmd.service`.

### 3.6 Tests
- Add integration tests for register/list/find/proxy flows.
- Add multi-process tests where `daffydmd` launches or monitors a child service.
- Add restart/recovery tests around persisted state and stale pid files.

Exit criteria:
- `daffydmd` can manage at least one service end to end and survive a restart with persisted state.

## Phase 4: Default Services Implementation

### 4.1 Service inventory
- Define the mandatory built-in services needed for a usable DaffyChat deployment.
- Split them into P0, P1, and example-only services.

### 4.2 First-party built-ins
- Implement at least the following built-in services:
  - health/status service;
  - room metadata/state service;
  - bot API service;
  - event-bus bridge service;
  - recipe import/export service;
  - artifact/service loader service.
- Place concrete implementations under `services/builtin/` or the chosen runtime source layout.

### 4.3 Service packaging and registration
- Ensure built-in services can be registered with `daffydmd` automatically.
- Add packaging/install rules for their specs, generated code, and runtime binaries.

### 4.4 Service contract tests
- Add contract tests for each built-in service’s RPC or REST surface.
- Verify registration metadata, schema validation, and failure behavior.

Exit criteria:
- a default deployment ships with a minimal useful set of working services, not just the sample echo slice.

## Phase 5: Room Lifecycle And Containment

### 5.1 Room registry completion
- Finish the room model, room registry, and lifecycle states.
- Add room create/start/stop/destroy operations.
- Persist room definitions and runtime metadata.

### 5.2 LXC integration
- Implement the intended `lxc`-based room containment flow.
- Define container rootfs, network model, volume layout, and per-room artifact directories.
- Ensure rooms cannot access the host beyond the documented allowances.

### 5.3 Room bootstrap pipeline
- Create the bootstrap sequence that launches a room, attaches default services, mounts config, and wires the event bus.
- Provide cleanup for partial startup failures.

### 5.4 Room lifecycle tests
- Add integration tests for room creation, room restart, room teardown, and failed bootstrap recovery.

Exit criteria:
- a room can be started and stopped in an isolated environment with its baseline runtime pieces attached.

## Phase 6: Event Bus, REST APIs, And Runtime Surface

### 6.1 Event-bus model
- Finalize room event schemas and naming rules.
- Ensure DSSL-defined events map cleanly to runtime publication and frontend consumption.

### 6.2 REST APIs
- Implement the default room REST APIs promised by the project brief.
- Wire request authentication/authorization decisions where required.
- Document the stable API contracts.

### 6.3 SSE/EventSource bridge
- Ensure frontend-side `EventSource` consumption matches the server event bus.
- Add reconnect, backfill, and malformed-event behavior where appropriate.

### 6.4 Runtime authorization
- Decide how rooms, services, bots, and operators authenticate to each other.
- Add at least a first pass of request validation and access control.

Exit criteria:
- runtime events and default REST APIs are usable from a room and from the frontend bridge.

## Phase 7: Frontend Completion

### 7.1 WASM artifact loading
- Build the UI and runtime plumbing needed to load Daffyscript-generated WASM artifacts into the frontend.
- Support artifact discovery, validation, load errors, and lifecycle hooks.

### 7.2 Room UX completion
- Finish room creation/join flows if still partial.
- Add UI affordances for loading recipes, activating extensions, and viewing service/runtime state.
- Surface errors from failed service calls or missing artifacts.

### 7.3 Voice-enabled room UX
- Ensure the frontend can drive the existing voice backend paths in a supported way.
- Add visible status for audio device, session, connection, and diagnostics.

### 7.4 Frontend tests
- Add browser or Node-based smoke tests for bridge bootstrap, event consumption, and WASM loading.
- Add at least one high-level integration test for a room page with an active extension.

Exit criteria:
- a user can open a room UI, receive live events, and load a Daffyscript-generated extension artifact.

## Phase 8: Extension System Completion

### 8.1 Lua runtime
- Finish the embedded Lua server-side addon surface.
- Define safe lifecycle hooks, APIs, and sandbox limitations.
- Add example Lua addons and tests.

### 8.2 Shared-library plugin API
- Finalize the plugin ABI/API surface.
- Add Doxygen-friendly docstrings to the plugin API headers.
- Add loader, unload, and compatibility checks.
- Create example shared-library plugins.

### 8.3 Daffyscript toolchain
- Expand the Daffyscript compiler beyond bootstrap parsing.
- Add a linter under `toolchain/`.
- Define the special DaffyChat-specific WASM contract and validate generated artifacts.

### 8.4 Recipe system
- Implement room recipe creation from Daffyscript.
- Implement room-image export/import so a configured room can be saved and restored.
- Add recipe validation and conflict handling.

### 8.5 Example extensions
- Grow `stdext/` toward the target of roughly 12 representative examples across service types.
- Cover DSSL services, Lua addons, Daffyscript, shared libraries, REST-based integrations, and event-bus listeners.

Exit criteria:
- every advertised extension surface has at least one working example and documented workflow.

## Phase 9: Configuration, Install, Deploy, And Packaging

### 9.1 Configuration model
- Finalize the JSON and `libconfuse` config schemas.
- Ensure both `/etc/daffychat/daffychat.json` and `/etc/daffychat/daffychat.conf` flows are supported as documented.
- Add config validation, defaults, and example files.

### 9.2 Artifact layout
- Standardize runtime directories, especially `/var/www/daffychat`, service run directories, logs, and packaged assets.
- Ensure install targets and runtime assumptions match.

### 9.3 Packaging targets
- Finish `scripts/archive-service.py` integration for DEB, RPM, Pacman, tarball, gzip, bzip2, and zlib outputs.
- Ensure generated archives include configs, services, docs, manpages, and the daemon-manager service unit.
- Validate the `LINK_STATIC` behavior and header-only dependency rules.

### 9.4 Deployment automation
- Finish and verify `scripts/deploy.sh` for a clean server install.
- Add idempotent deploy behavior, upgrade behavior, and rollback notes.

### 9.5 Installer tests
- Add package-content checks and at least one install smoke test path.
- Verify config selection behavior driven by `DAFFYCHAT_CONFIG_CONFUSE` and `DAFFYCHAT_CONFIG_JSON`.

Exit criteria:
- a clean host can install, configure, and start DaffyChat using the documented package/deploy path.

## Phase 10: Documentation, Manpages, And Operator Guidance

### 10.1 Core docs
- Finish `INSTALL.md`, `GUIDE.md`, `DEPLOY.md`, `EXTENSIBILITY.md`, `RECIPES.md`, and `README.md` so they reflect actual supported behavior.
- Add missing architecture documentation under `docs/`.

### 10.2 API and extension docs
- Generate Doxygen docs for the plugin API.
- Document DSSL service authoring, Lua addon authoring, Daffyscript workflows, recipe authoring, and shared-library development.

### 10.3 Manpages
- Write manpages for:
  - `daffychat` or the main operator entrypoint;
  - `daffydmd`;
  - `dssl-bindgen`;
  - `dssl-docgen`;
  - `dssl-docstrip`;
  - `daffyscript`;
  - deploy/package helpers where appropriate.
- Wire them into install targets and package outputs.

### 10.4 Troubleshooting content
- Add runbook-style docs for logs, pid files, stale sockets, service crashes, container failures, and config mistakes.

Exit criteria:
- developers and operators can install, extend, debug, and package DaffyChat using repo docs alone.

## Phase 11: Test Matrix And Quality Gates

### 11.1 Unit-test coverage
- Raise coverage around services, runtime state, IPC, event bus, config parsing, and extension loaders.

### 11.2 Integration-test coverage
- Add realistic tests for generated services, `daffydmd`, room bootstrapping, REST APIs, event streaming, and frontend bridge flows.

### 11.3 End-to-end scenarios
- Add a small number of full-system scenarios, for example:
  - start daemon manager;
  - bring up one room;
  - register one generated service;
  - send one event;
  - load one frontend artifact;
  - export and re-import one recipe.

### 11.4 CI-quality gates
- Define the must-pass build/test/doc/package jobs.
- Add formatting/linting hooks where missing.
- Fail merges when generated artifacts or docs drift.

Exit criteria:
- the repo has trustworthy automated checks for the supported developer, runtime, and operator paths.

## Phase 12: Build-System Parity And Release Engineering

### 12.1 CMake completion
- Finish any missing options for installing docs, examples, toolchains, and generated services.
- Verify packaging and install targets from a clean tree.

### 12.2 Meson parity
- Bring `meson.build` and related files to feature parity with the final CMake feature set.
- Ensure generated services, packaging knobs, tests, and install layout are represented.

### 12.3 Autotools parity
- Add the promised Autotools build once the canonical feature set is stable.
- Keep it aligned with CMake/Meson rather than implementing divergent behavior.

### 12.4 Release process
- Define versioning, changelog generation, release artifact signing if desired, and release verification steps.

Exit criteria:
- CMake is production-ready, and alternate build systems match the supported feature set closely enough for release.

## Recommended Execution Order Inside The Current Repo

### Immediate P0
- Finish the DSSL-generated service path.
- Finish the real `nng` transport path.
- Finish the minimal `daffydmd` with persisted service state and brokered request flow.
- Turn the current echo slice into a real generated-service example used by tests and packaging.

### Short-term P1
- Implement the first built-in services beyond echo.
- Complete room lifecycle and event-bus wiring.
- Complete frontend WASM loading and recipe import/export.
- Add package/install validation.

### Medium-term P2
- Complete Lua, shared-library plugin, and Daffyscript extension workflows.
- Expand docs, manpages, and examples.
- Add LMDB recovery, multi-process supervision, and end-to-end scenarios.

### Final P3
- Reach build-system parity for Meson and Autotools.
- Perform release hardening, cleanup, and final documentation polish.

## Suggested Milestone Breakdown

### Milestone A: Service Runtime Bootstrap
- DSSL generation works.
- Echo service is fully generated and tested.
- `nng` request/reply and pub/sub are real and stable.
- Minimal `daffydmd` can broker one service.

### Milestone B: Room Runtime Usable
- Room lifecycle works.
- Event bus and REST APIs work.
- Default services are present.
- Frontend can connect to a live room.

### Milestone C: Extensions Usable
- Daffyscript WASM loads.
- Lua and plugin examples work.
- Recipes can export/import room state.

### Milestone D: Operator Release
- Packages install cleanly.
- Docs and manpages are complete.
- Deploy workflow works on a clean host.
- CI and release checks pass.

## Risks To Manage Throughout

- Generator drift between checked-in artifacts and build-produced artifacts.
- IPC race conditions and environment-specific socket behavior.
- Complexity creep in `daffydmd` before the minimum viable flow is stable.
- Containerization issues that block room runtime validation.
- Extension-surface sprawl without enough tests or examples.
- Documentation getting ahead of implementation and becoming misleading.

## Non-Negotiable Acceptance Checklist

Before calling DaffyChat finished, verify all of these:
- `dssl-bindgen` can generate a usable service from a real DSSL spec.
- `daffydmd` can supervise and broker at least one service.
- room lifecycle works with containment.
- frontend can receive room events and load a Daffyscript-produced WASM artifact.
- package/install/deploy flow works on a clean system.
- docs/manpages/examples match actual behavior.
- automated tests cover the critical runtime and operator workflows.
