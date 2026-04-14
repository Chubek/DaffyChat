# DaffyChat: Ephemeral, Extensibke, Voice-Enabled Chatrooms

DaffyChat, written in C++, rooms are ephemeral, extensible, and void-enabled. There are several ways with which we can extend a DaffyChat room, including DaffyChat Services, WASM programs, or Lua scripts. DaffyChat backend is decoupled from its frontend, but it's shipped with a minimal frontend that uses Alpine.js and Pico.css.

Every DaffyChat room get its own container to run on, provided by the LXC library. Therefore, the microservices users add to their room, or the scripts, do not harm the server. These microservices can add new APIs to the server, because the Daffy Service Specification Language (DSSL) is capable of specifying REST APIs.

Furthermore, each room has its own event bus, and webhooks. The webhooks API is built on the back of the event bus, and the event bus is exposed to the user in the WASM and Lua programs, so they can utilize it to provide futher expansions to the room.

DaffyChat rooms come with several default APIs. One of these APIs is a Bot API. Bots are communicated with using special messages beginning with a `/`. These messages can also be used to communicate with WASM and Lua scripts. Lua scripts in a chatroom have several libraries, all begiging with `ldc`. For example, `ldcevent` library is used to communicate with the event bus, and `ldcmessage` intercepts messages, and `ldcservice`communicate with the microservices.

WASM, however, is a different story. We offer WASM addons for extending the frontend. Each DaffyChat frontend needs to take some steps to make this possible. The WASM module is loaded server-side, but it modifies the frontend through an extension bridge. The server-side WASM module emits events, that the decoupled frontend must [optionally] comply with through hooks. So, each conformant DaffyChat frontend must implement methods to react to WASM addon message and event bus. DaffyChat implements a C library that would help interacting with these buses. Bindings for this library has been provided through SWIG.

DaffyChat comes with Daffyscript, a language that exclusively compiles to WASM. If you wish to write WASM plugins for DaffyChat, Daffyscript is your best choice.

For writing microservices, we use the Daffy Service Specification Language. DSSL can specify data structures, enumerations, remote procedure calls, REST APIs, execute system commands, and so on. DSSL is extensible itself, through shared libraries. The API is expsed at `dssl/include/dssl-plugin.h`.

The Bot API, and other APIs, are created using DSSL. We offer other APIs: User & Identity API, Room State & Environment API, Storage API, Message Formatting API, Void & Audio API, Timer & Scheduling API, Crypto & Security API, HTTP Client API, Frontend Extension API, File & Media Upload API, and Metrics & Analytics API. All these is defined in DSSL.

Remember that DSSL is just a declarative language. We define the actual logic in C++. If we have a service called `echo`, we run `dssl-bindgen --cpp echo.dssl`, and it generates the following skeleton files:

- `echo_api.hpp`
- `echo_api.cpp`
- `echo_api.lua` (bindings)
- `build_echo.sh`
- `deploy_echo.sh`
- `echo.json` (metadata, can be made to generate YAML as well)
- `echo_doc.json` (extracted DSSL docstrings, can be made to generate YAML as well)

Note: keep your DSSL files well-documented using docstrings. You can use the `dssl-docstrip` to extract the docstrings at any time, and you can use `dssl-docgen` to compile this JSON or YAML file into HTML or LaTeX.

`dssl-bindgen` generates Lua bindings (which can be turned off) for any target. The supported targets are: Go, Rust, Python, Ruby. `dssl-bindgen` first generates an AST of the target. You can define a new target by using `dssl-binding`'s S-Expression-based target definition tool. You can either pass this new target via CLI, or place it in `$XDG_CONFIG_HOME/DaffyChat/dssl-targets`. If you wanna know how the intermediary S-Expression for a DSSL file looks like, pass the `--gen-ast` flag to it.

When you run `build_*.sh`, the file will be compiled into a "Daemon Archive", e.g. `echo.da`. The room admin can then run `/exec deloy_echo.sh` to deploy the service. You actually need to restart the room's state when you deploy a new service.

Finally, you can always create a room with a "Room Recipe". This recipe will allow you to add any extension you want, and modify the room for provenance. These recipies are written using Daffyscript, but you need to declare at the start of the script that you're writing a recipe.

