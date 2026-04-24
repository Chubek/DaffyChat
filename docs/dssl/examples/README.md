# DSSL Examples

Real-world DSSL service definitions from the DaffyChat codebase.

## Available Examples

### Basic Services

- [echo.dssl](echo.dssl) - Simple single-RPC echo service
- [health.dssl](health.dssl) - Health check service pattern

### Multi-RPC Services

- [room_ops.dssl](room_ops.dssl) - Room operations with multiple endpoints
- [bot_api.dssl](bot_api.dssl) - Complex bot integration API

### Patterns

- **Single RPC** - Services with one method (echo, health)
- **Multi-RPC** - Services with multiple methods (room_ops, bot_api)
- **CRUD Operations** - Create, Read, Update, Delete patterns (bot_api)
- **Event Streaming** - Cursor-based event polling (bot_api)
- **Authentication** - Token-based auth patterns (bot_api)

## Example Categories

### 1. Simple Services

Services with minimal complexity, ideal for learning:

- `echo.dssl` - Basic request/response
- `health.dssl` - Status reporting

### 2. Operational Services

Services for room and user management:

- `room_ops.dssl` - Join/leave operations
- `room_state.dssl` - State management

### 3. Integration Services

Services for external integrations:

- `bot_api.dssl` - Bot registration and messaging
- `event_bridge.dssl` - Event routing

## Usage

Each example includes:

1. **Complete DSSL specification**
2. **Generated code structure**
3. **Implementation notes**
4. **Usage examples**

## Learning Path

1. Start with `echo.dssl` - Understand basic structure
2. Study `room_ops.dssl` - Learn multi-RPC patterns
3. Explore `bot_api.dssl` - See complex service design
4. Review implementation notes for each example

## Running Examples

All examples are production services in DaffyChat:

```bash
# Generate code
./toolchain/dssl-bindgen.py --target cpp --out-dir ./gen services/specs/echo.dssl

# Build service
cd build && cmake --build . -j2

# Run service
./build/echo-service
```

## Next Steps

- Read each example's documentation
- Compare DSSL specs with generated code
- Study implementation patterns
- Create your own services based on these examples
