Introduction
============

What is DaffyChat?
------------------

DaffyChat is a modern, extensible voice chatroom platform designed for real-time communication with a focus on:

* **Voice-first architecture** - Native WebRTC peer-to-peer voice with high-quality audio processing
* **Service-oriented design** - Microservices defined in DSSL with automatic code generation
* **Extensibility** - WASM-based extensions via Daffyscript for custom room logic
* **Hybrid client model** - Native clients for voice, web frontend for room management

Key Features
------------

Voice Communication
~~~~~~~~~~~~~~~~~~~

* Peer-to-peer WebRTC voice transport
* High-quality audio with Opus codec
* Noise suppression via RNNoise
* SRTP encryption for secure media

Service Architecture
~~~~~~~~~~~~~~~~~~~~

* DSSL (DaffyChat Service Definition Language) for type-safe service definitions
* Automatic C++ code generation from service specs
* IPC transport via NNG for inter-service communication
* Daemon lifecycle management with ``daffydmd``

Extension System
~~~~~~~~~~~~~~~~

* Daffyscript language compiles to WebAssembly
* Server-hosted extension execution
* Event-driven architecture with room-scoped extensions
* Safe sandboxed execution environment

Frontend
~~~~~~~~

* React-based web interface
* Room management and participant tracking
* Extension bridge for server-side events
* Real-time state synchronization

Architecture Overview
---------------------

DaffyChat follows a multi-plane architecture:

**Control Plane**
  Backend services handle room lifecycle, participant tracking, and service orchestration.

**Media Plane**
  Native voice stack with libdatachannel, libsrtp, PortAudio, and Opus for peer-to-peer voice.

**Extension Plane**
  DSSL microservices and Daffyscript WASM extensions provide customizable room logic.

**Presentation Plane**
  Web frontend consumes server events and provides user interface.

Use Cases
---------

* **Voice chat rooms** - Create persistent or ephemeral voice channels
* **Bot integration** - Automated agents via Bot API service
* **Custom room logic** - Daffyscript extensions for moderation, commands, games
* **Event-driven workflows** - React to room events with custom handlers

Technology Stack
----------------

**Backend (C++)**
  * libdatachannel - WebRTC peer connections
  * libsrtp - Secure RTP encryption
  * PortAudio - Audio I/O
  * Opus - Audio codec
  * RNNoise - Noise suppression
  * NNG - IPC transport
  * nlohmann/json - JSON serialization

**Frontend (JavaScript/TypeScript)**
  * React - UI framework
  * Next.js - Web framework
  * WebAssembly - Extension runtime

**Toolchain**
  * CMake/Meson/Ninja - Build systems
  * Python - Toolchain scripts
  * DSSL compiler - Service code generation
  * Daffyscript compiler - WASM compilation

Getting Started
---------------

See :doc:`installation` for build instructions and :doc:`quickstart` for a quick tour of DaffyChat's features.

For developers, start with:

* :doc:`dssl/getting-started` - Create your first DSSL service
* :doc:`daffyscript/index` - Learn Daffyscript for extensions
* :doc:`development/building` - Build from source
