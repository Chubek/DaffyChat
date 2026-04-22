# Daffyscript Compiler Guide

The Daffyscript compiler (`daffyscript`) compiles `.dfy`, `.dfyp`, and `.dfyr` files to WebAssembly.

## Installation

The compiler is built as part of DaffyChat:

```bash
cmake -S . -B build
cmake --build build --target daffyscript
```

The binary will be at `build/daffyscript`.

## Basic Usage

```bash
daffyscript source.dfy
```

This compiles `source.dfy` to `source.wasm`.

## Command-Line Options

### Specify Output File

```bash
daffyscript --out output.wasm source.dfy
```

### Override File Type

```bash
daffyscript --target module source.dfy
daffyscript --target program source.dfyp
daffyscript --target recipe source.dfyr
```

Valid targets: `module`, `program`, `recipe`

### Optimization Level

```bash
daffyscript --opt 0 source.dfy  # No optimization
daffyscript --opt 1 source.dfy  # Basic optimization (default)
daffyscript --opt 2 source.dfy  # Aggressive optimization
daffyscript --opt 3 source.dfy  # Maximum optimization
```

### Emit AST

Dump the abstract syntax tree as JSON:

```bash
daffyscript --emit-ast source.dfy
```

### Validate Only

Check syntax and semantics without generating code:

```bash
daffyscript --validate source.dfy
```

### Suppress Standard Library

```bash
daffyscript --no-stdlib source.dfy
```

### Help

```bash
daffyscript --help
```

## Complete Example

```bash
daffyscript \
  --target module \
  --opt 2 \
  --out my_extension.wasm \
  source.dfy
```

## Compilation Process

1. **Lexical Analysis** - Source code → tokens
2. **Parsing** - Tokens → AST
3. **Semantic Analysis** - Type checking, validation
4. **Code Generation** - AST → WASM bytecode
5. **Optimization** - WASM optimization passes
6. **Output** - Write `.wasm` file

## Error Messages

The compiler provides detailed error messages:

```
Error: source.dfy:10:5: Type mismatch
  Expected: int
  Got: str
  
  let x: int = "hello"
               ^~~~~~~
```

## Diagnostics

### Syntax Errors

```
Error: source.dfy:5:10: Unexpected token
  Expected: '}'
  Got: 'fn'
```

### Type Errors

```
Error: source.dfy:15:8: Cannot call non-function
  Type: int
  
  let x = 42
  x()
  ^
```

### Semantic Errors

```
Error: source.dfy:20:5: Undefined variable 'foo'
  
  let x = foo
          ^~~
```

## Integration with Build Systems

### CMake

```cmake
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/extension.wasm
  COMMAND daffyscript --out ${CMAKE_CURRENT_BINARY_DIR}/extension.wasm
          ${CMAKE_CURRENT_SOURCE_DIR}/extension.dfy
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/extension.dfy
)
```

### Make

```makefile
%.wasm: %.dfy
	daffyscript --opt 2 --out $@ $<
```

### Shell Script

```bash
#!/bin/bash
for file in src/*.dfy; do
  daffyscript --opt 2 "$file"
done
```

## Debugging

### Check AST

```bash
daffyscript --emit-ast source.dfy | jq .
```

### Validate Without Compiling

```bash
daffyscript --validate source.dfy
```

### Verbose Output

Set environment variable:

```bash
DAFFYSCRIPT_VERBOSE=1 daffyscript source.dfy
```

## Performance Tips

### Optimization Levels

- `--opt 0` - Fast compilation, larger output
- `--opt 1` - Balanced (default)
- `--opt 2` - Slower compilation, smaller output
- `--opt 3` - Slowest compilation, smallest output

### Incremental Compilation

The compiler doesn't cache results. Use build system caching:

```bash
# Only recompile if source changed
make extension.wasm
```

## Troubleshooting

### Compiler Crashes

```bash
# Get stack trace
DAFFYSCRIPT_DEBUG=1 daffyscript source.dfy
```

### Invalid WASM Output

```bash
# Validate WASM
wasm-validate output.wasm
```

### Import Errors

Check import paths are correct:

```daffyscript
import dfc_bridge_sse  // Correct
import dfc.bridge.sse  // Wrong
```

## File Extensions

- `.dfy` - Module source
- `.dfyp` - Program source
- `.dfyr` - Recipe source
- `.wasm` - Compiled output

## Exit Codes

- `0` - Success
- `1` - Compilation error
- `2` - Invalid arguments

## Environment Variables

- `DAFFYSCRIPT_VERBOSE` - Enable verbose output
- `DAFFYSCRIPT_DEBUG` - Enable debug output
- `DAFFYSCRIPT_STDLIB_PATH` - Override stdlib location

## Version Information

```bash
daffyscript --version
```

## Examples

### Compile Module

```bash
daffyscript hello.dfy
# Produces: hello.wasm
```

### Compile Program with Optimization

```bash
daffyscript --opt 3 --out bot.wasm standup_helper.dfyp
```

### Validate Recipe

```bash
daffyscript --validate incident_bridge.dfyr
```

### Batch Compilation

```bash
for f in modules/*.dfy; do
  daffyscript --opt 2 "$f"
done
```
