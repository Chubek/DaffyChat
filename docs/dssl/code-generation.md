# DSSL Code Generation

Understanding how DSSL specifications compile to C++ service implementations.

## Overview

The `dssl-bindgen` tool transforms DSSL specifications into production-ready C++ code with:

- Type-safe service interfaces
- JSON-RPC 2.0 request/response handling
- Automatic serialization/deserialization
- Multi-RPC method dispatch
- IPC transport integration

## Generated Files

For a service `echo.dssl`, the generator produces:

```
generated/
├── echo_service.hpp    # Service interface and types
└── echo_service.cpp    # Implementation skeleton
```

## Type Generation

### Structs

DSSL structs become C++ structs with JSON serialization:

**DSSL:**
```dssl
struct EchoRequest {
    message: string;
    sender: string;
}
```

**Generated C++:**
```cpp
struct EchoRequest {
    std::string message;
    std::string sender;
    
    // JSON serialization
    nlohmann::json to_json() const {
        return {
            {"message", message},
            {"sender", sender}
        };
    }
    
    // JSON deserialization
    static EchoRequest from_json(const nlohmann::json& j) {
        EchoRequest req;
        req.message = j.at("message").get<std::string>();
        req.sender = j.at("sender").get<std::string>();
        return req;
    }
};
```

### Arrays

DSSL arrays become `std::vector<T>`:

**DSSL:**
```dssl
struct BotRecord {
    capabilities: string[];
    room_scope: string[];
}
```

**Generated C++:**
```cpp
struct BotRecord {
    std::vector<std::string> capabilities;
    std::vector<std::string> room_scope;
    
    nlohmann::json to_json() const {
        return {
            {"capabilities", capabilities},
            {"room_scope", room_scope}
        };
    }
};
```

## Service Interface Generation

### Single-RPC Service

**DSSL:**
```dssl
service echo 1.0.0;

rpc Echo(message: string, sender: string) returns EchoReply;
```

**Generated Interface:**
```cpp
class EchoService {
public:
    virtual ~EchoService() = default;
    
    // Pure virtual method to implement
    virtual EchoReply Echo(const std::string& message, 
                           const std::string& sender) = 0;
    
    // JSON-RPC request handler
    std::string handle_request(const std::string& request_json);
    
    // Service metadata
    static constexpr const char* service_name = "echo";
    static constexpr const char* service_version = "1.0.0";
};
```

### Multi-RPC Service

**DSSL:**
```dssl
service roomops 1.0.0;

rpc Join(user: string) returns JoinReply;
rpc Leave(user: string) returns LeaveReply;
```

**Generated Interface:**
```cpp
class RoomOpsService {
public:
    virtual ~RoomOpsService() = default;
    
    // Pure virtual methods
    virtual JoinReply Join(const std::string& user) = 0;
    virtual LeaveReply Leave(const std::string& user) = 0;
    
    // Automatic dispatch based on 'method' field
    std::string handle_request(const std::string& request_json);
};
```

## Request Handling

The generated `handle_request` method:

1. **Parses JSON-RPC request**
2. **Validates structure** (jsonrpc, method, params, id)
3. **Dispatches to correct RPC method**
4. **Serializes response**
5. **Handles errors** (invalid method, missing params, exceptions)

**Generated Implementation:**
```cpp
std::string EchoService::handle_request(const std::string& request_json) {
    try {
        auto req = nlohmann::json::parse(request_json);
        
        // Validate JSON-RPC 2.0 structure
        if (!req.contains("jsonrpc") || req["jsonrpc"] != "2.0") {
            return error_response(-32600, "Invalid Request", req["id"]);
        }
        
        std::string method = req.at("method").get<std::string>();
        auto params = req.at("params");
        auto id = req["id"];
        
        // Dispatch to RPC method
        if (method == "Echo") {
            std::string message = params.at("message").get<std::string>();
            std::string sender = params.at("sender").get<std::string>();
            
            EchoReply reply = Echo(message, sender);
            
            return nlohmann::json{
                {"jsonrpc", "2.0"},
                {"result", reply.to_json()},
                {"id", id}
            }.dump();
        }
        
        return error_response(-32601, "Method not found", id);
        
    } catch (const std::exception& e) {
        return error_response(-32603, e.what(), nullptr);
    }
}
```

## Multi-RPC Dispatch

For services with multiple RPCs, the generator creates a dispatch table:

```cpp
std::string RoomOpsService::handle_request(const std::string& request_json) {
    // ... validation ...
    
    std::string method = req.at("method").get<std::string>();
    
    if (method == "Join") {
        std::string user = params.at("user").get<std::string>();
        JoinReply reply = Join(user);
        return success_response(reply.to_json(), id);
    }
    else if (method == "Leave") {
        std::string user = params.at("user").get<std::string>();
        LeaveReply reply = Leave(user);
        return success_response(reply.to_json(), id);
    }
    
    return error_response(-32601, "Method not found", id);
}
```

## Error Handling

The generator includes standard JSON-RPC 2.0 error codes:

| Code | Message | Meaning |
|------|---------|---------|
| -32700 | Parse error | Invalid JSON |
| -32600 | Invalid Request | Missing required fields |
| -32601 | Method not found | Unknown RPC method |
| -32602 | Invalid params | Wrong parameter types |
| -32603 | Internal error | Exception during execution |

**Error Response Format:**
```json
{
    "jsonrpc": "2.0",
    "error": {
        "code": -32601,
        "message": "Method not found"
    },
    "id": 1
}
```

## Implementation Skeleton

The generator creates a skeleton implementation:

```cpp
#include "echo_service.hpp"

namespace daffy {

class EchoServiceImpl : public EchoService {
public:
    EchoReply Echo(const std::string& message, 
                   const std::string& sender) override {
        // TODO: Implement service logic
        EchoReply reply;
        reply.message = message;
        reply.sender = sender;
        reply.echoed = true;
        return reply;
    }
};

} // namespace daffy
```

## Integration with IPC Transport

Generated services integrate with `nng` transport:

```cpp
#include "echo_service.hpp"
#include <daffy/ipc/nng_transport.hpp>

int main() {
    daffy::EchoServiceImpl service;
    daffy::NngTransport transport("/tmp/daffy-echo.ipc");
    
    transport.bind();
    
    // Service loop: receive request, call handle_request, send response
    transport.serve([&service](const std::string& request) {
        return service.handle_request(request);
    });
}
```

## Namespace Customization

Use `--namespace` flag to customize the generated namespace:

```bash
dssl-bindgen --target cpp --namespace myapp --out-dir ./gen spec.dssl
```

Generates:
```cpp
namespace myapp {
    class EchoService { ... };
}
```

## Validation Mode

Validate DSSL syntax without generating code:

```bash
dssl-bindgen --validate-only spec.dssl
```

Checks:
- Syntax correctness
- Type references
- Naming conventions
- Duplicate definitions

## Build Integration

Add generated code to CMake:

```cmake
# Generate service code
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/echo_service.cpp
    COMMAND dssl-bindgen 
        --target cpp 
        --out-dir ${CMAKE_BINARY_DIR}/generated
        ${CMAKE_SOURCE_DIR}/services/specs/echo.dssl
    DEPENDS ${CMAKE_SOURCE_DIR}/services/specs/echo.dssl
)

# Build service
add_executable(echo-service
    ${CMAKE_BINARY_DIR}/generated/echo_service.cpp
    src/services/echo_main.cpp
)
```

## Next Steps

- See [Best Practices](best-practices.md) for design patterns
- See [Examples](examples/) for real-world services
- Read [Language Reference](language-reference.md) for complete syntax
