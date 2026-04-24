# Implementation Notes

Detailed notes on implementing the example DSSL services.

## Echo Service

**File:** `echo.dssl`

### Generated Code Structure

```cpp
// echo_service.hpp
namespace daffy {
    struct EchoRequest { ... };
    struct EchoReply { ... };
    
    class EchoService {
        virtual EchoReply Echo(const std::string& message, 
                               const std::string& sender) = 0;
        std::string handle_request(const std::string& json);
    };
}
```

### Implementation Example

```cpp
class EchoServiceImpl : public EchoService {
public:
    EchoReply Echo(const std::string& message, 
                   const std::string& sender) override {
        EchoReply reply;
        reply.message = message;
        reply.sender = sender;
        reply.service_name = "echo";
        reply.echoed = true;
        return reply;
    }
};
```

### JSON-RPC Request

```json
{
    "jsonrpc": "2.0",
    "method": "Echo",
    "params": {
        "message": "Hello, world!",
        "sender": "alice"
    },
    "id": 1
}
```

### JSON-RPC Response

```json
{
    "jsonrpc": "2.0",
    "result": {
        "message": "Hello, world!",
        "sender": "alice",
        "service_name": "echo",
        "echoed": true
    },
    "id": 1
}
```

### Key Concepts

- **Single RPC**: Service has one method
- **Struct parameters**: Parameters are primitives, but could use EchoRequest struct
- **Metadata in response**: service_name and echoed flag provide context

---

## Room Operations Service

**File:** `room_ops.dssl`

### Generated Code Structure

```cpp
// room_ops_service.hpp
namespace daffy {
    struct JoinReply { ... };
    struct LeaveReply { ... };
    
    class RoomOpsService {
        virtual JoinReply Join(const std::string& user) = 0;
        virtual LeaveReply Leave(const std::string& user) = 0;
        std::string handle_request(const std::string& json);
    };
}
```

### Implementation Example

```cpp
class RoomOpsServiceImpl : public RoomOpsService {
private:
    std::set<std::string> participants_;
    
public:
    JoinReply Join(const std::string& user) override {
        participants_.insert(user);
        
        JoinReply reply;
        reply.user = user;
        reply.action = "join";
        reply.service_name = "roomops";
        return reply;
    }
    
    LeaveReply Leave(const std::string& user) override {
        participants_.erase(user);
        
        LeaveReply reply;
        reply.user = user;
        reply.action = "leave";
        reply.service_name = "roomops";
        return reply;
    }
};
```

### Method Dispatch

The generated `handle_request` method dispatches based on the `method` field:

```cpp
std::string RoomOpsService::handle_request(const std::string& json) {
    auto req = nlohmann::json::parse(json);
    std::string method = req.at("method").get<std::string>();
    
    if (method == "Join") {
        std::string user = req.at("params").at("user").get<std::string>();
        JoinReply reply = Join(user);
        return success_response(reply.to_json(), req["id"]);
    }
    else if (method == "Leave") {
        std::string user = req.at("params").at("user").get<std::string>();
        LeaveReply reply = Leave(user);
        return success_response(reply.to_json(), req["id"]);
    }
    
    return error_response(-32601, "Method not found", req["id"]);
}
```

### JSON-RPC Requests

**Join:**
```json
{
    "jsonrpc": "2.0",
    "method": "Join",
    "params": {"user": "alice"},
    "id": 1
}
```

**Leave:**
```json
{
    "jsonrpc": "2.0",
    "method": "Leave",
    "params": {"user": "alice"},
    "id": 2
}
```

### Key Concepts

- **Multi-RPC**: Service has multiple methods
- **Automatic dispatch**: Generated code routes to correct method
- **Consistent structure**: Both replies have similar fields

---

## Bot API Service

**File:** `bot_api.dssl`

### Generated Code Structure

```cpp
// bot_api_service.hpp
namespace daffy {
    struct BotRecord { ... };
    struct BotToken { ... };
    struct BotSession { ... };
    struct RoomEvent { ... };
    // ... more structs ...
    
    class BotApiService {
        virtual BotRecord RegisterBot(...) = 0;
        virtual BotRecord GetBot(...) = 0;
        virtual std::vector<BotRecord> ListBots(...) = 0;
        virtual BotSession JoinRoom(...) = 0;
        // ... more methods ...
        std::string handle_request(const std::string& json);
    };
}
```

### Implementation Example

```cpp
class BotApiServiceImpl : public BotApiService {
private:
    std::map<std::string, BotRecord> bots_;
    std::map<std::string, std::string> tokens_;  // token -> bot_id
    std::map<std::string, BotSession> sessions_;
    
public:
    BotRecord RegisterBot(const std::string& display_name,
                          const std::vector<std::string>& capabilities,
                          const std::vector<std::string>& room_scope) override {
        std::string bot_id = generate_uuid();
        std::string token = generate_token();
        
        BotRecord record;
        record.bot_id = bot_id;
        record.display_name = display_name;
        record.enabled = true;
        record.created_at = current_timestamp();
        record.updated_at = current_timestamp();
        record.capabilities = capabilities;
        record.room_scope = room_scope;
        
        bots_[bot_id] = record;
        tokens_[token] = bot_id;
        
        return record;
    }
    
    BotRecord GetBot(const std::string& bot_id) override {
        auto it = bots_.find(bot_id);
        if (it == bots_.end()) {
            throw std::runtime_error("Bot not found");
        }
        return it->second;
    }
    
    std::vector<BotRecord> ListBots(bool enabled,
                                    const std::string& room_id,
                                    const std::string& capability) override {
        std::vector<BotRecord> results;
        
        for (const auto& [id, bot] : bots_) {
            // Apply filters
            if (enabled && !bot.enabled) continue;
            if (!room_id.empty() && !has_room_scope(bot, room_id)) continue;
            if (!capability.empty() && !has_capability(bot, capability)) continue;
            
            results.push_back(bot);
        }
        
        return results;
    }
    
    BotSession JoinRoom(const std::string& token,
                        const std::string& room_id) override {
        std::string bot_id = validate_token(token);
        std::string session_id = generate_uuid();
        
        BotSession session;
        session.session_id = session_id;
        session.bot_id = bot_id;
        session.room_id = room_id;
        session.joined_at = current_timestamp();
        session.last_seen_at = current_timestamp();
        session.state = "active";
        
        sessions_[session_id] = session;
        
        return session;
    }
    
    std::vector<RoomEvent> PollEvents(const std::string& token,
                                      const std::string& room_id,
                                      double after_sequence,
                                      double limit) override {
        std::string bot_id = validate_token(token);
        
        // Fetch events from event store
        return event_store_.get_events(room_id, 
                                       static_cast<int64_t>(after_sequence),
                                       static_cast<size_t>(limit));
    }
};
```

### JSON-RPC Examples

**RegisterBot:**
```json
{
    "jsonrpc": "2.0",
    "method": "RegisterBot",
    "params": {
        "display_name": "ModBot",
        "capabilities": ["moderation", "commands"],
        "room_scope": []
    },
    "id": 1
}
```

**ListBots:**
```json
{
    "jsonrpc": "2.0",
    "method": "ListBots",
    "params": {
        "enabled": true,
        "room_id": "",
        "capability": "moderation"
    },
    "id": 2
}
```

**PollEvents:**
```json
{
    "jsonrpc": "2.0",
    "method": "PollEvents",
    "params": {
        "token": "eyJhbGc...",
        "room_id": "room-123",
        "after_sequence": 42,
        "limit": 10
    },
    "id": 3
}
```

### Key Concepts

- **Complex service**: 10+ RPC methods
- **CRUD operations**: Create (RegisterBot), Read (GetBot, ListBots)
- **Authentication**: Token-based auth pattern
- **Array returns**: ListBots, PollEvents return arrays
- **Cursor-based streaming**: PollEvents uses sequence numbers
- **State management**: Tracks bots, tokens, sessions

---

## Health Service

**File:** `health.dssl`

### Implementation Example

```cpp
class HealthServiceImpl : public HealthService {
private:
    std::chrono::steady_clock::time_point start_time_;
    
public:
    HealthServiceImpl() : start_time_(std::chrono::steady_clock::now()) {}
    
    HealthStatus Check() override {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time_
        ).count();
        
        HealthStatus status;
        status.healthy = true;
        status.service_name = "health";
        status.version = "1.0.0";
        status.uptime_seconds = static_cast<double>(uptime);
        status.timestamp = current_timestamp();
        
        return status;
    }
};
```

### JSON-RPC Request

```json
{
    "jsonrpc": "2.0",
    "method": "Check",
    "params": {},
    "id": 1
}
```

### Key Concepts

- **No parameters**: RPC takes no arguments
- **Monitoring pattern**: Common for health checks
- **Operational metrics**: Uptime, version, timestamp

---

## Common Patterns

### Error Handling

All services should handle errors consistently:

```cpp
try {
    // Service logic
} catch (const std::invalid_argument& e) {
    return error_response(-32602, "Invalid params: " + std::string(e.what()), id);
} catch (const std::runtime_error& e) {
    return error_response(-32603, "Internal error: " + std::string(e.what()), id);
}
```

### Token Validation

For authenticated services:

```cpp
std::string validate_token(const std::string& token) {
    auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        throw std::runtime_error("Invalid token");
    }
    return it->second;  // Return bot_id
}
```

### Timestamp Generation

Use ISO 8601 format:

```cpp
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}
```

### UUID Generation

For unique identifiers:

```cpp
std::string generate_uuid() {
    // Use boost::uuids or similar
    return "uuid-" + std::to_string(rand());
}
```

## Testing

### Unit Tests

Test each RPC method independently:

```cpp
TEST(EchoService, Echo) {
    EchoServiceImpl service;
    auto reply = service.Echo("hello", "alice");
    
    EXPECT_EQ(reply.message, "hello");
    EXPECT_EQ(reply.sender, "alice");
    EXPECT_TRUE(reply.echoed);
}
```

### Integration Tests

Test JSON-RPC handling:

```cpp
TEST(EchoService, HandleRequest) {
    EchoServiceImpl service;
    
    std::string request = R"({
        "jsonrpc": "2.0",
        "method": "Echo",
        "params": {"message": "hello", "sender": "alice"},
        "id": 1
    })";
    
    std::string response = service.handle_request(request);
    auto json = nlohmann::json::parse(response);
    
    EXPECT_EQ(json["jsonrpc"], "2.0");
    EXPECT_EQ(json["id"], 1);
    EXPECT_EQ(json["result"]["message"], "hello");
}
```

## Next Steps

- Study the generated code for each example
- Implement your own service based on these patterns
- Add error handling and validation
- Write comprehensive tests
