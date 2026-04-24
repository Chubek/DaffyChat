# Getting Started with DSSL

This tutorial walks you through creating your first DSSL service from scratch.

## Prerequisites

- DaffyChat repository cloned and built
- `dssl-bindgen` binary available in `build/`
- Basic understanding of RPC concepts

## Step 1: Create a Service Specification

Create a new file `services/specs/greeter.dssl`:

```dssl
/// Simple greeting service
service greeter 1.0.0;

/// Request payload for greeting
struct GreetRequest {
    name: string;
    language: string;
}

/// Response payload with greeting
struct GreetReply {
    greeting: string;
    language: string;
}

/// Generate a greeting in the specified language
rpc Greet(name: string, language: string) returns GreetReply;
```

## Step 2: Generate Service Code

Use the `dssl-bindgen.py` wrapper to generate C++ code:

```bash
./toolchain/dssl-bindgen.py \
    --target cpp \
    --out-dir ./generated \
    services/specs/greeter.dssl
```

This generates:
- `generated/greeter_service.hpp` - Service interface
- `generated/greeter_service.cpp` - Service implementation skeleton

## Step 3: Implement Service Logic

Edit `generated/greeter_service.cpp` to add your business logic:

```cpp
#include "greeter_service.hpp"
#include <map>

namespace daffy {

class GreeterServiceImpl : public GreeterService {
public:
    GreetReply Greet(const std::string& name, const std::string& language) override {
        static const std::map<std::string, std::string> greetings = {
            {"en", "Hello"},
            {"es", "Hola"},
            {"fr", "Bonjour"},
            {"de", "Guten Tag"}
        };
        
        GreetReply reply;
        reply.language = language;
        
        auto it = greetings.find(language);
        if (it != greetings.end()) {
            reply.greeting = it->second + ", " + name + "!";
        } else {
            reply.greeting = "Hello, " + name + "!";
        }
        
        return reply;
    }
};

} // namespace daffy
```

## Step 4: Build the Service

Add your service to `CMakeLists.txt`:

```cmake
add_executable(greeter-service
    generated/greeter_service.cpp
    src/services/greeter_main.cpp
)

target_link_libraries(greeter-service
    daffy_ipc
    nng::nng
)
```

Create the main entry point `src/services/greeter_main.cpp`:

```cpp
#include "greeter_service.hpp"
#include <daffy/ipc/nng_transport.hpp>
#include <iostream>

int main(int argc, char** argv) {
    daffy::GreeterServiceImpl service;
    daffy::NngTransport transport("/tmp/daffy-greeter.ipc");
    
    if (!transport.bind()) {
        std::cerr << "Failed to bind transport\n";
        return 1;
    }
    
    std::cout << "Greeter service listening on /tmp/daffy-greeter.ipc\n";
    
    // Service loop
    transport.serve([&service](const std::string& request) {
        return service.handle_request(request);
    });
    
    return 0;
}
```

Build:

```bash
cd build
cmake --build . -j2
```

## Step 5: Register with Daemon Manager

Use the installation helper:

```bash
./toolchain/install-service.py \
    --name greeter \
    --binary ./build/greeter-service \
    --socket /tmp/daffy-greeter.ipc
```

This registers the service with `daffydmd` for lifecycle management.

## Step 6: Start the Service

```bash
./build/daffydmd start greeter
```

Check status:

```bash
./build/daffydmd status greeter
```

## Step 7: Test the Service

Create a simple client:

```cpp
#include <daffy/ipc/nng_transport.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

int main() {
    daffy::NngTransport client;
    client.connect("/tmp/daffy-greeter.ipc");
    
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"method", "Greet"},
        {"params", {
            {"name", "Alice"},
            {"language", "es"}
        }},
        {"id", 1}
    };
    
    std::string response = client.send(request.dump());
    std::cout << "Response: " << response << "\n";
    
    return 0;
}
```

## Using the Scaffolding Tool

For faster development, use `dssl-init.py` to scaffold everything:

```bash
./toolchain/dssl-init.py \
    --name greeter \
    --version 1.0.0 \
    --rpc Greet
```

This creates:
- `services/specs/greeter.dssl` - Service specification
- `src/services/greeter_service.cpp` - Implementation skeleton
- `src/services/greeter_main.cpp` - Entry point
- CMakeLists.txt entry (manual addition required)

## Multi-RPC Services

To create a service with multiple endpoints:

```dssl
service calculator 1.0.0;

struct CalcResult {
    result: number;
    operation: string;
}

rpc Add(a: number, b: number) returns CalcResult;
rpc Subtract(a: number, b: number) returns CalcResult;
rpc Multiply(a: number, b: number) returns CalcResult;
rpc Divide(a: number, b: number) returns CalcResult;
```

The generated code automatically dispatches to the correct method based on the `method` field in the JSON-RPC request.

## Best Practices

1. **Version your services** - Use semantic versioning
2. **Document everything** - Use `///` comments liberally
3. **Keep structs focused** - Single responsibility per type
4. **Use descriptive names** - Clear parameter and field names
5. **Handle errors gracefully** - Return error codes or status fields
6. **Test incrementally** - Verify each RPC method independently

## Next Steps

- Read [Code Generation](code-generation.md) to understand the generated code
- See [Best Practices](best-practices.md) for design patterns
- Explore [Examples](examples/) for real-world services
