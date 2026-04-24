# DSSL Documentation

**DSSL** (DaffyChat Service Definition Language) is a declarative IDL for defining microservices in the DaffyChat ecosystem. It generates type-safe C++ service implementations with automatic IPC transport, serialization, and daemon lifecycle management.

## Documentation Index

- [Language Reference](language-reference.md) - Complete DSSL syntax and semantics
- [Getting Started](getting-started.md) - Quick tutorial for creating your first service
- [Code Generation](code-generation.md) - How DSSL compiles to C++ services
- [Best Practices](best-practices.md) - Design patterns and conventions
- [Examples](examples/) - Real-world DSSL service definitions

## Quick Example

```dssl
/// Simple echo service
service echo 1.0.0;

struct EchoRequest {
    message: string;
    sender: string;
}

struct EchoReply {
    message: string;
    echoed: bool;
}

rpc Echo(message: string, sender: string) returns EchoReply;
```

## Key Features

- **Type-safe RPC definitions** - Strongly typed request/response contracts
- **Automatic code generation** - Produces complete C++ service skeletons
- **Built-in IPC transport** - Uses `nng` for inter-process communication
- **Daemon lifecycle management** - Integrates with `daffydmd` for service orchestration
- **Multi-RPC services** - Single service can expose multiple endpoints
- **Semantic versioning** - Services declare version compatibility

## Toolchain

- `dssl-bindgen` - Core code generator (C++ binary)
- `toolchain/dssl-bindgen.py` - User-friendly CLI wrapper
- `toolchain/dssl-init.py` - Service scaffolding tool
- `toolchain/install-service.py` - Daemon installation helper

## Architecture

DSSL services run as independent daemons managed by `daffydmd`. Each service:

1. Listens on a Unix domain socket (`/tmp/daffy-<service>.ipc`)
2. Implements generated C++ interfaces
3. Handles JSON-RPC 2.0 requests over `nng` transport
4. Registers with the daemon manager for lifecycle control

## Next Steps

- Read the [Language Reference](language-reference.md) for complete syntax
- Follow the [Getting Started](getting-started.md) tutorial
- Explore [Examples](examples/) for real service definitions
