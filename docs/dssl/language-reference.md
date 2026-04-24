# DSSL Language Reference

Complete syntax and semantics for the DaffyChat Service Definition Language.

## File Structure

A DSSL file consists of:

1. **Service declaration** (required, exactly one)
2. **Type definitions** (optional, zero or more structs/enums)
3. **RPC definitions** (required, one or more)

```dssl
/// Documentation comment
service <name> <version>;

/// Type definitions
struct <TypeName> { ... }
enum <EnumName> { ... }

/// RPC definitions
rpc <MethodName>(...) returns <ReturnType>;
```

## Service Declaration

Every DSSL file must start with a service declaration:

```dssl
service <name> <version>;
```

- **name**: Lowercase identifier (alphanumeric + underscore)
- **version**: Semantic version string (e.g., `1.0.0`, `2.1.3`)

**Example:**
```dssl
service echo 1.0.0;
service room_ops 2.0.1;
service botapi 1.0.0;
```

## Comments

DSSL supports documentation comments using triple-slash syntax:

```dssl
/// This is a documentation comment
/// It can span multiple lines
service myservice 1.0.0;

/// Request payload for echo operation
struct EchoRequest {
    message: string;  /// The message to echo
}
```

Comments are preserved in generated code as C++ doc comments.

## Data Types

### Primitive Types

| DSSL Type | C++ Type | Description |
|-----------|----------|-------------|
| `string` | `std::string` | UTF-8 text |
| `number` | `double` | Floating-point number |
| `bool` | `bool` | Boolean value |

### Array Types

Arrays are denoted with `[]` suffix:

```dssl
struct Example {
    tags: string[];
    scores: number[];
    flags: bool[];
}
```

Generated as `std::vector<T>` in C++.

### Struct Types

User-defined composite types:

```dssl
struct TypeName {
    field_name: type;
}
```

**Example:**
```dssl
struct BotRecord {
    bot_id: string;
    display_name: string;
    enabled: bool;
    created_at: string;
    capabilities: string[];
}
```

**Rules:**
- Struct names must be PascalCase
- Field names must be snake_case
- Fields cannot be optional (all fields required)
- Structs can reference other structs

### Enum Types

Named enumeration types (future feature, not yet implemented):

```dssl
enum Status {
    PENDING,
    ACTIVE,
    COMPLETED,
    FAILED
}
```

## RPC Definitions

RPC methods define the service's public API:

```dssl
rpc <MethodName>(<param>: <type>, ...) returns <ReturnType>;
```

### Parameters

- **Named parameters**: `param_name: type`
- **Multiple parameters**: Comma-separated
- **No parameters**: Empty parentheses `()`

**Examples:**
```dssl
rpc Echo(message: string, sender: string) returns EchoReply;
rpc Status() returns string;
rpc ListBots(enabled: bool, room_id: string) returns BotRecord[];
```

### Return Types

Return types can be:

- **Primitive types**: `string`, `number`, `bool`
- **Struct types**: User-defined structs
- **Array types**: `Type[]`
- **Void**: Use `bool` for success/failure operations

**Examples:**
```dssl
rpc GetBot(bot_id: string) returns BotRecord;
rpc ListBots() returns BotRecord[];
rpc DeleteBot(bot_id: string) returns bool;
rpc GetStatus() returns string;
```

### Multi-RPC Services

Services can define multiple RPC methods. The generated code includes automatic dispatch logic:

```dssl
service roomops 1.0.0;

rpc Join(user: string) returns JoinReply;
rpc Leave(user: string) returns LeaveReply;
rpc ListParticipants() returns string[];
```

## Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Service name | snake_case | `echo`, `room_ops`, `bot_api` |
| Struct name | PascalCase | `EchoRequest`, `BotRecord` |
| Field name | snake_case | `bot_id`, `display_name` |
| RPC method | PascalCase | `Echo`, `RegisterBot`, `JoinRoom` |
| Parameter name | snake_case | `message`, `room_id` |

## Complete Example

```dssl
/// Multi-RPC room operations service
service roomops 1.0.0;

/// Reply payload for join operation
struct JoinReply {
    user: string;
    action: string;
    service_name: string;
}

/// Reply payload for leave operation
struct LeaveReply {
    user: string;
    action: string;
    service_name: string;
}

/// Join a room as a user
rpc Join(user: string) returns JoinReply;

/// Leave a room as a user
rpc Leave(user: string) returns LeaveReply;
```

## Validation Rules

The DSSL parser enforces:

1. **Service declaration must be first** (after comments)
2. **Exactly one service declaration** per file
3. **At least one RPC definition** required
4. **Struct fields cannot be empty** (at least one field)
5. **No duplicate names** (structs, RPCs, fields)
6. **Valid identifiers** (alphanumeric + underscore)
7. **Type references must exist** (no undefined types)

## Reserved Keywords

The following are reserved and cannot be used as identifiers:

- `service`
- `struct`
- `enum`
- `rpc`
- `returns`
- `string`
- `number`
- `bool`

## Next Steps

- See [Getting Started](getting-started.md) for a tutorial
- See [Code Generation](code-generation.md) for implementation details
- See [Examples](examples/) for real-world service definitions
