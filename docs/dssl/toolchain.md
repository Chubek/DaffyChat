# DSSL Toolchain

Complete guide to the DSSL toolchain and CLI utilities.

## Overview

The DSSL toolchain consists of:

1. **dssl-bindgen** - Core code generator (C++ binary)
2. **dssl-bindgen.py** - User-friendly CLI wrapper
3. **dssl-init.py** - Service scaffolding tool
4. **install-service.py** - Daemon installation helper

## dssl-bindgen

The core code generator that transforms DSSL specifications into C++ code.

### Location

```
build/dssl-bindgen
```

Built as part of the main CMake build process.

### Usage

```bash
dssl-bindgen --target cpp --out-dir <output> <spec.dssl>
```

### Options

| Option | Description | Required |
|--------|-------------|----------|
| `--target` | Target language (currently only `cpp`) | Yes |
| `--out-dir` | Output directory for generated files | Yes |
| `--namespace` | Custom C++ namespace (default: `daffy`) | No |
| `--validate-only` | Only validate spec, don't generate code | No |

### Examples

**Generate C++ service:**
```bash
dssl-bindgen --target cpp --out-dir ./generated services/specs/echo.dssl
```

**Custom namespace:**
```bash
dssl-bindgen --target cpp --namespace myapp --out-dir ./gen spec.dssl
```

**Validate only:**
```bash
dssl-bindgen --validate-only services/specs/echo.dssl
```

## dssl-bindgen.py

Python wrapper that provides a more user-friendly interface to `dssl-bindgen`.

### Location

```
toolchain/dssl-bindgen.py
```

### Features

- Automatically finds the `dssl-bindgen` binary
- Validates input files before generation
- Creates output directories automatically
- Provides helpful error messages

### Usage

```bash
./toolchain/dssl-bindgen.py --target cpp --out-dir <output> <spec.dssl>
```

### Options

Same as `dssl-bindgen`, plus:

| Option | Description |
|--------|-------------|
| `--verbose` | Enable verbose output |
| `-v` | Short form of `--verbose` |

### Examples

**Generate with verbose output:**
```bash
./toolchain/dssl-bindgen.py -v --target cpp --out-dir ./gen echo.dssl
```

**Validate spec:**
```bash
./toolchain/dssl-bindgen.py --validate-only echo.dssl
```

## dssl-init.py

Scaffolding tool that creates complete service projects from templates.

### Location

```
toolchain/dssl-init.py
```

### Usage

```bash
./toolchain/dssl-init.py --name <service> --version <version> [options]
```

### Options

| Option | Description | Required |
|--------|-------------|----------|
| `--name` | Service name (snake_case) | Yes |
| `--version` | Service version (semver) | Yes |
| `--rpc` | RPC method name (can be repeated) | No |
| `--out-dir` | Output directory (default: current) | No |

### Generated Files

```
<service-name>/
├── specs/
│   └── <service>.dssl          # DSSL specification
├── src/
│   ├── <service>_service.cpp   # Implementation skeleton
│   └── <service>_main.cpp      # Entry point
└── README.md                    # Service documentation
```

### Examples

**Create simple service:**
```bash
./toolchain/dssl-init.py --name greeter --version 1.0.0 --rpc Greet
```

**Create multi-RPC service:**
```bash
./toolchain/dssl-init.py \
    --name calculator \
    --version 1.0.0 \
    --rpc Add \
    --rpc Subtract \
    --rpc Multiply \
    --rpc Divide
```

**Custom output directory:**
```bash
./toolchain/dssl-init.py \
    --name myservice \
    --version 1.0.0 \
    --out-dir ./services/myservice
```

## install-service.py

Helper tool for registering services with the daemon manager.

### Location

```
toolchain/install-service.py
```

### Usage

```bash
./toolchain/install-service.py --name <service> --binary <path> --socket <path>
```

### Options

| Option | Description | Required |
|--------|-------------|----------|
| `--name` | Service name | Yes |
| `--binary` | Path to service binary | Yes |
| `--socket` | IPC socket path | Yes |
| `--autostart` | Enable autostart on daemon launch | No |

### Examples

**Register service:**
```bash
./toolchain/install-service.py \
    --name echo \
    --binary ./build/echo-service \
    --socket /tmp/daffy-echo.ipc
```

**Register with autostart:**
```bash
./toolchain/install-service.py \
    --name echo \
    --binary ./build/echo-service \
    --socket /tmp/daffy-echo.ipc \
    --autostart
```

## Workflow Examples

### Creating a New Service

**Step 1: Scaffold the service**
```bash
./toolchain/dssl-init.py --name greeter --version 1.0.0 --rpc Greet
```

**Step 2: Edit the DSSL spec**
```bash
vim greeter/specs/greeter.dssl
```

**Step 3: Generate C++ code**
```bash
./toolchain/dssl-bindgen.py \
    --target cpp \
    --out-dir ./greeter/generated \
    greeter/specs/greeter.dssl
```

**Step 4: Implement service logic**
```bash
vim greeter/src/greeter_service.cpp
```

**Step 5: Build the service**
```bash
cd build
cmake --build . -j2
```

**Step 6: Register with daemon manager**
```bash
./toolchain/install-service.py \
    --name greeter \
    --binary ./build/greeter-service \
    --socket /tmp/daffy-greeter.ipc
```

**Step 7: Start the service**
```bash
./build/daffydmd start greeter
```

### Updating an Existing Service

**Step 1: Modify DSSL spec**
```bash
vim services/specs/echo.dssl
```

**Step 2: Regenerate code**
```bash
./toolchain/dssl-bindgen.py \
    --target cpp \
    --out-dir ./generated \
    services/specs/echo.dssl
```

**Step 3: Update implementation**
```bash
vim src/services/echo_service.cpp
```

**Step 4: Rebuild**
```bash
cd build && cmake --build . -j2
```

**Step 5: Restart service**
```bash
./build/daffydmd restart echo
```

### Validating Multiple Specs

```bash
for spec in services/specs/*.dssl; do
    echo "Validating $spec..."
    ./toolchain/dssl-bindgen.py --validate-only "$spec"
done
```

## Integration with CMake

### Automatic Code Generation

Add custom commands to regenerate code when specs change:

```cmake
# Generate service code
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/echo_service.cpp
           ${CMAKE_BINARY_DIR}/generated/echo_service.hpp
    COMMAND ${CMAKE_SOURCE_DIR}/toolchain/dssl-bindgen.py
        --target cpp
        --out-dir ${CMAKE_BINARY_DIR}/generated
        ${CMAKE_SOURCE_DIR}/services/specs/echo.dssl
    DEPENDS ${CMAKE_SOURCE_DIR}/services/specs/echo.dssl
            dssl-bindgen
    COMMENT "Generating echo service from DSSL"
)

# Build service
add_executable(echo-service
    ${CMAKE_BINARY_DIR}/generated/echo_service.cpp
    src/services/echo_main.cpp
)

target_include_directories(echo-service PRIVATE
    ${CMAKE_BINARY_DIR}/generated
)
```

### Batch Generation

Generate all services at once:

```cmake
file(GLOB DSSL_SPECS "${CMAKE_SOURCE_DIR}/services/specs/*.dssl")

foreach(SPEC ${DSSL_SPECS})
    get_filename_component(SERVICE_NAME ${SPEC} NAME_WE)
    
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/generated/${SERVICE_NAME}_service.cpp
        COMMAND ${CMAKE_SOURCE_DIR}/toolchain/dssl-bindgen.py
            --target cpp
            --out-dir ${CMAKE_BINARY_DIR}/generated
            ${SPEC}
        DEPENDS ${SPEC} dssl-bindgen
    )
endforeach()
```

## Troubleshooting

### Binary Not Found

**Error:**
```
Error: dssl-bindgen binary not found
```

**Solution:**
```bash
cd build
cmake --build . -j2
```

### Invalid DSSL Syntax

**Error:**
```
Parse error at line 5: unexpected token 'rcp'
```

**Solution:**
Check for typos in keywords (`rpc`, `struct`, `service`, etc.)

### Missing Output Directory

**Error:**
```
Error: Output directory does not exist
```

**Solution:**
The Python wrapper creates directories automatically. If using the binary directly:
```bash
mkdir -p ./generated
```

### Permission Denied

**Error:**
```
Permission denied: ./toolchain/dssl-bindgen.py
```

**Solution:**
```bash
chmod +x ./toolchain/dssl-bindgen.py
```

## Next Steps

- Read [Getting Started](getting-started.md) for a complete tutorial
- See [Language Reference](language-reference.md) for DSSL syntax
- Explore [Examples](examples/) for real-world usage
