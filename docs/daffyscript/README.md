# Daffyscript Language Documentation

Daffyscript is a domain-specific language designed for extending DaffyChat rooms with custom behavior, automation, and frontend enhancements. It compiles to WebAssembly and runs in a sandboxed environment with controlled access to DaffyChat APIs.

## Overview

Daffyscript supports three distinct file types, each serving a different purpose:

- **Modules** (`.dfy`) - Frontend extensions that run in the browser
- **Programs** (`.dfyp`) - Server-side room automation and bots
- **Recipes** (`.dfyr`) - Room configuration and assembly documents

All three types compile to WebAssembly and integrate with the DaffyChat runtime through a well-defined event and hook system.

## Quick Start

### Installation

The Daffyscript compiler is built as part of the DaffyChat build:

```bash
cmake -S . -B build
cmake --build build --target daffyscript
```

### Your First Module

Create a file `hello.dfy`:

```daffyscript
module hello_world
version 1.0.0

pub fn greet(name: str) {
    emit "message:send" {
        text: "Hello, " + name + "!",
    }
}

on hook "room:participant-joined" (payload: {peer_id: str, name: str}) {
    greet(payload.name)
}

exports {
    greet,
}
```

Compile it:

```bash
./build/daffyscript hello.dfy
```

This produces `hello.wasm` which can be loaded in the DaffyChat frontend.

## Language Features

### Type System

Daffyscript has a static type system with the following built-in types:

- `str` - UTF-8 strings
- `int` - 64-bit signed integers
- `float` - 64-bit floating point
- `bool` - Boolean values (`true`, `false`)
- `bytes` - Byte arrays

Composite types:

- `[T]` - Lists of type T
- `{K: V}` - Maps from K to V
- `T?` - Optional type (nullable)
- Custom structs and enums

### Variables and Constants

```daffyscript
let x = 42                    // Immutable binding
let mut y = "hello"           // Mutable binding
y = "world"                   // Reassignment

let name: str = "Alice"       // Explicit type annotation
let items: [int] = [1, 2, 3]  // List type
```

### Functions

```daffyscript
fn add(a: int, b: int) -> int {
    return a + b
}

pub fn public_function() {
    // Public functions are exported
}

fn no_return_type() {
    // Implicitly returns none
}
```

### Control Flow

```daffyscript
// If-else
if condition {
    // then block
} else {
    // else block
}

// For loops
for item in items {
    // iterate over items
}

for i, item in items {
    // iterate with index
}

// While loops
while condition {
    // loop body
}

// Match expressions
match value {
    pattern1 => {
        // arm 1
    }
    pattern2 => {
        // arm 2
    }
}
```

### Operators

**Arithmetic:** `+`, `-`, `*`, `/`, `%`

**Comparison:** `==`, `!=`, `<`, `>`, `<=`, `>=`

**Logical:** `and`, `or`, `not`

**String:** `+` (concatenation)

**Null coalescing:** `??`

### Structs and Enums

```daffyscript
struct User {
    id: str,
    name: str,
    age: int,
}

enum Status {
    Active,
    Inactive,
    Pending,
}

let user = User {
    id: "123",
    name: "Alice",
    age: 30,
}
```

### Error Handling

```daffyscript
try {
    // code that might fail
} catch err {
    // handle error
    raise err  // re-raise
}
```

## Documentation Index

- [Language Reference](language-reference.md) - Complete syntax and semantics
- [Modules](modules.md) - Frontend extensions
- [Programs](programs.md) - Server-side automation
- [Recipes](recipes.md) - Room configuration
- [Standard Library](stdlib.md) - Built-in functions and modules
- [Compiler Guide](compiler.md) - Using the Daffyscript compiler
- [Examples](examples.md) - Sample code and tutorials
- [API Reference](api-reference.md) - DaffyChat integration APIs

## File Extensions

- `.dfy` - Module files (frontend)
- `.dfyp` - Program files (server-side)
- `.dfyr` - Recipe files (configuration)
- `.wasm` - Compiled WebAssembly output

## Next Steps

- Read the [Language Reference](language-reference.md) for complete syntax
- Explore [Examples](examples.md) for practical code samples
- Learn about [Modules](modules.md) to create frontend extensions
- Check the [Standard Library](stdlib.md) for available APIs
