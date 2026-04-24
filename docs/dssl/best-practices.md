# DSSL Best Practices

Design patterns and conventions for building robust DSSL services.

## Service Design

### Keep Services Focused

Each service should have a single, well-defined responsibility:

**Good:**
```dssl
service user_auth 1.0.0;

rpc Login(username: string, password: string) returns AuthToken;
rpc Logout(token: string) returns bool;
rpc ValidateToken(token: string) returns bool;
```

**Avoid:**
```dssl
service everything 1.0.0;

rpc Login(...) returns AuthToken;
rpc SendEmail(...) returns bool;
rpc ProcessPayment(...) returns Receipt;
rpc GenerateReport(...) returns string;
```

### Use Semantic Versioning

Version services according to API compatibility:

- **Major version** (1.x.x → 2.x.x): Breaking changes
- **Minor version** (x.1.x → x.2.x): New features, backward compatible
- **Patch version** (x.x.1 → x.x.2): Bug fixes

```dssl
service botapi 1.0.0;  // Initial release
service botapi 1.1.0;  // Added new RPC methods
service botapi 2.0.0;  // Changed parameter types (breaking)
```

## Type Design

### Use Descriptive Struct Names

Struct names should clearly indicate their purpose:

**Good:**
```dssl
struct BotRegistrationRequest {
    display_name: string;
    capabilities: string[];
}

struct BotRegistrationResponse {
    bot_id: string;
    token: string;
}
```

**Avoid:**
```dssl
struct Data {
    name: string;
    stuff: string[];
}

struct Result {
    id: string;
    key: string;
}
```

### Group Related Fields

Organize struct fields logically:

```dssl
struct BotRecord {
    // Identity
    bot_id: string;
    display_name: string;
    
    // State
    enabled: bool;
    
    // Timestamps
    created_at: string;
    updated_at: string;
    
    // Configuration
    capabilities: string[];
    room_scope: string[];
}
```

### Use Consistent Field Naming

Follow snake_case for all field names:

**Good:**
```dssl
struct UserProfile {
    user_id: string;
    display_name: string;
    created_at: string;
}
```

**Avoid:**
```dssl
struct UserProfile {
    userId: string;        // camelCase
    DisplayName: string;   // PascalCase
    created_at: string;    // Inconsistent
}
```

## RPC Design

### Use Action Verbs for Method Names

RPC methods should start with verbs:

**Good:**
```dssl
rpc RegisterBot(...) returns BotRecord;
rpc GetBot(...) returns BotRecord;
rpc ListBots(...) returns BotRecord[];
rpc DeleteBot(...) returns bool;
```

**Avoid:**
```dssl
rpc Bot(...) returns BotRecord;
rpc Bots(...) returns BotRecord[];
rpc BotDeletion(...) returns bool;
```

### Return Structured Types

Prefer returning structs over primitives for extensibility:

**Good:**
```dssl
struct StatusResponse {
    healthy: bool;
    uptime_seconds: number;
    active_connections: number;
}

rpc GetStatus() returns StatusResponse;
```

**Avoid:**
```dssl
rpc GetStatus() returns string;  // Hard to extend
```

### Use Arrays for Collections

Return arrays for multiple items:

```dssl
rpc ListBots(enabled: bool) returns BotRecord[];
rpc GetRoomEvents(room_id: string, limit: number) returns RoomEvent[];
```

### Include Metadata in Responses

Add context to response structs:

```dssl
struct BotRecord {
    bot_id: string;
    display_name: string;
    enabled: bool;
    
    // Metadata
    created_at: string;
    updated_at: string;
    version: number;
}
```

## Error Handling

### Use Boolean Returns for Simple Operations

For operations that can only succeed or fail:

```dssl
rpc DeleteBot(bot_id: string) returns bool;
rpc EnableBot(bot_id: string) returns bool;
```

### Include Status Fields in Complex Operations

For operations with multiple outcomes:

```dssl
struct OperationResult {
    success: bool;
    error_code: string;
    error_message: string;
    affected_count: number;
}

rpc BulkDeleteBots(bot_ids: string[]) returns OperationResult;
```

### Document Error Conditions

Use comments to describe failure modes:

```dssl
/// Delete a bot by ID
/// Returns false if bot_id does not exist or deletion fails
rpc DeleteBot(bot_id: string) returns bool;
```

## Documentation

### Document Everything

Use triple-slash comments liberally:

```dssl
/// Bot API service for automated agent integration
service botapi 1.0.0;

/// Bot registration record with metadata
struct BotRecord {
    bot_id: string;           /// Unique bot identifier
    display_name: string;     /// Human-readable bot name
    enabled: bool;            /// Whether bot is active
}

/// Register a new bot and issue authentication token
/// Returns BotRecord with generated bot_id
rpc RegisterBot(display_name: string, capabilities: string[]) returns BotRecord;
```

### Explain Non-Obvious Behavior

Document side effects and special cases:

```dssl
/// Join a room as an authenticated bot
/// Creates a new session and subscribes to room events
/// Returns BotSession with session_id for subsequent operations
rpc JoinRoom(token: string, room_id: string) returns BotSession;
```

## Service Organization

### Separate Concerns

Split large services into focused modules:

```
services/specs/
├── auth.dssl          # Authentication
├── user_profile.dssl  # User management
├── room_ops.dssl      # Room operations
└── bot_api.dssl       # Bot integration
```

### Use Consistent Naming

Follow patterns across related services:

```dssl
// User operations
service user_ops 1.0.0;
rpc CreateUser(...) returns UserRecord;
rpc GetUser(...) returns UserRecord;
rpc ListUsers(...) returns UserRecord[];

// Room operations
service room_ops 1.0.0;
rpc CreateRoom(...) returns RoomRecord;
rpc GetRoom(...) returns RoomRecord;
rpc ListRooms(...) returns RoomRecord[];
```

## Performance Considerations

### Limit Array Sizes

Add pagination for large collections:

```dssl
struct PagedResult {
    items: BotRecord[];
    total_count: number;
    page: number;
    page_size: number;
}

rpc ListBots(page: number, page_size: number) returns PagedResult;
```

### Use Cursors for Event Streams

For sequential data access:

```dssl
rpc PollEvents(
    room_id: string, 
    after_sequence: number,  // Cursor
    limit: number
) returns RoomEvent[];
```

### Avoid Deep Nesting

Keep struct hierarchies flat:

**Good:**
```dssl
struct BotRecord {
    bot_id: string;
    display_name: string;
}

struct BotSession {
    session_id: string;
    bot_id: string;  // Reference, not nested
}
```

**Avoid:**
```dssl
struct BotSession {
    session_id: string;
    bot: BotRecord;  // Nested, harder to serialize
}
```

## Testing

### Create Test Services

Use simple services for testing infrastructure:

```dssl
service echo 1.0.0;

struct EchoReply {
    message: string;
    echoed: bool;
}

rpc Echo(message: string) returns EchoReply;
```

### Test Each RPC Independently

Verify each method in isolation:

```cpp
TEST(RoomOpsService, Join) {
    RoomOpsServiceImpl service;
    auto reply = service.Join("alice");
    EXPECT_EQ(reply.user, "alice");
    EXPECT_EQ(reply.action, "join");
}

TEST(RoomOpsService, Leave) {
    RoomOpsServiceImpl service;
    auto reply = service.Leave("alice");
    EXPECT_EQ(reply.user, "alice");
    EXPECT_EQ(reply.action, "leave");
}
```

## Migration Strategies

### Deprecate Gracefully

When changing APIs, support both versions:

```dssl
service myservice 2.0.0;

/// New method (preferred)
rpc GetUserV2(user_id: string) returns UserRecordV2;

/// Deprecated: Use GetUserV2 instead
rpc GetUser(user_id: string) returns UserRecord;
```

### Version Struct Types

Create new types for breaking changes:

```dssl
struct UserRecordV1 {
    user_id: string;
    name: string;
}

struct UserRecordV2 {
    user_id: string;
    display_name: string;  // Renamed field
    email: string;         // New field
}
```

## Security

### Validate Input

Always validate parameters in implementation:

```cpp
BotRecord RegisterBot(const std::string& display_name, 
                      const std::vector<std::string>& capabilities) override {
    if (display_name.empty()) {
        throw std::invalid_argument("display_name cannot be empty");
    }
    
    if (capabilities.empty()) {
        throw std::invalid_argument("capabilities cannot be empty");
    }
    
    // ... implementation ...
}
```

### Use Token-Based Authentication

For authenticated operations:

```dssl
rpc JoinRoom(token: string, room_id: string) returns BotSession;
rpc PostMessage(token: string, room_id: string, text: string) returns string;
```

### Sanitize String Inputs

Prevent injection attacks in implementation:

```cpp
std::string sanitize(const std::string& input) {
    // Remove control characters, validate UTF-8, etc.
}
```

## Next Steps

- See [Language Reference](language-reference.md) for complete syntax
- See [Code Generation](code-generation.md) for implementation details
- Explore [Examples](examples/) for real-world patterns
