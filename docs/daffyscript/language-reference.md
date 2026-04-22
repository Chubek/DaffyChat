# Daffyscript Language Reference

Complete syntax and semantics reference for the Daffyscript language.

## File Structure

Every Daffyscript file begins with a declaration specifying its type:

```daffyscript
module <name>           // Frontend module
program <name>          // Server-side program
recipe "<name>"         // Room recipe
```

Followed by metadata:

```daffyscript
version 1.0.0
author "Your Name"      // Optional
description "..."       // Optional
```

## Lexical Structure

### Comments

```daffyscript
-- Single-line comment
```

### Identifiers

Identifiers start with a letter or underscore, followed by letters, digits, or underscores:

```
identifier = [a-zA-Z_][a-zA-Z0-9_]*
```

### Literals

**Integer literals:**
```daffyscript
42
-17
0
```

**Float literals:**
```daffyscript
3.14
-0.5
2.0
```

**String literals:**
```daffyscript
"hello world"
"escaped \"quotes\""
```

**Boolean literals:**
```daffyscript
true
false
```

**None literal:**
```daffyscript
none
```

### Keywords

Reserved keywords cannot be used as identifiers:

```
let, mut, fn, return, if, else, for, while, match, in
struct, enum, type, import, pub, true, false, none
and, or, not, try, catch, raise
module, emit, expect, hook, exports
program, command, intercept, message, every, at, timezone, room, event
recipe, service, version, author, description, config, autostart
roles, role, can, cannot, default_role
webhooks, post, to, headers, when, on, init
```

## Type System

### Built-in Types

- `str` - UTF-8 string
- `int` - 64-bit signed integer
- `float` - 64-bit floating point
- `bool` - Boolean
- `bytes` - Byte array

### Composite Types

**List type:**
```daffyscript
[T]              // List of T
[int]            // List of integers
[[str]]          // List of list of strings
```

**Map type:**
```daffyscript
{K: V}           // Map from K to V
{str: int}       // String to integer map
```

**Optional type:**
```daffyscript
T?               // Optional T (can be none)
str?             // Optional string
```

### User-Defined Types

**Struct:**
```daffyscript
struct Point {
    x: int,
    y: int,
}

struct User {
    id: str,
    name: str,
    age: int,
    email: str?,
}
```

**Enum:**
```daffyscript
enum Color {
    Red,
    Green,
    Blue,
}

enum Status {
    Active,
    Inactive,
    Pending,
}
```

## Expressions

### Literals

```daffyscript
42                  // Integer
3.14                // Float
"hello"             // String
true                // Boolean
none                // None
```

### Identifiers

```daffyscript
variable_name
function_name
```

### Dotted Expressions

```daffyscript
module.function
object.field
nested.path.value
```

### Binary Operations

**Arithmetic:**
```daffyscript
a + b               // Addition
a - b               // Subtraction
a * b               // Multiplication
a / b               // Division
a % b               // Modulo
```

**Comparison:**
```daffyscript
a == b              // Equal
a != b              // Not equal
a < b               // Less than
a > b               // Greater than
a <= b              // Less than or equal
a >= b              // Greater than or equal
```

**Logical:**
```daffyscript
a and b             // Logical AND
a or b              // Logical OR
```

**String concatenation:**
```daffyscript
"hello" + " " + "world"
```

**Null coalescing:**
```daffyscript
value ?? default    // Use default if value is none
```

### Unary Operations

```daffyscript
-x                  // Negation
not condition       // Logical NOT
```

### Function Calls

```daffyscript
function()
function(arg1, arg2)
module.function(arg)
```

### Method Calls

```daffyscript
object.method()
object.method(arg1, arg2)
```

### Indexing

```daffyscript
list[0]
map["key"]
```

### Field Access

```daffyscript
struct.field
object.property
```

### List Literals

```daffyscript
[]                  // Empty list
[1, 2, 3]          // Integer list
["a", "b", "c"]    // String list
```

### Map Literals

```daffyscript
{}                  // Empty map
{"key": "value"}   // Single entry
{"a": 1, "b": 2}   // Multiple entries
```

### Struct Initialization

```daffyscript
Point { x: 10, y: 20 }

User {
    id: "123",
    name: "Alice",
    age: 30,
    email: none,
}
```

## Statements

### Let Statement

```daffyscript
let x = 42                    // Immutable
let mut y = "hello"           // Mutable
let name: str = "Alice"       // With type annotation
```

### Assignment

```daffyscript
x = 42
object.field = value
list[0] = item
```

### Return Statement

```daffyscript
return
return value
return x + y
```

### Raise Statement

```daffyscript
raise error
raise "Error message"
```

### Expression Statement

```daffyscript
function()
emit "event" { field: value }
```

### If Statement

```daffyscript
if condition {
    // then block
}

if condition {
    // then block
} else {
    // else block
}
```

### For Statement

```daffyscript
for item in items {
    // iterate over items
}

for i, item in items {
    // iterate with index
}
```

### While Statement

```daffyscript
while condition {
    // loop body
}
```

### Match Statement

```daffyscript
match value {
    pattern1 => {
        // arm 1
    }
    pattern2 => {
        // arm 2
    }
}
```

### Try-Catch Statement

```daffyscript
try {
    // code that might fail
} catch err {
    // handle error
}
```

### Emit Statement

```daffyscript
emit "event-name" {
    field1: value1,
    field2: value2,
}
```

## Declarations

### Function Declaration

```daffyscript
fn function_name(param1: type1, param2: type2) -> return_type {
    // function body
}

pub fn public_function() {
    // exported function
}
```

### Struct Declaration

```daffyscript
struct StructName {
    field1: type1,
    field2: type2,
}
```

### Enum Declaration

```daffyscript
enum EnumName {
    Variant1,
    Variant2,
    Variant3,
}
```

### Import Declaration

```daffyscript
import module_name
import module.submodule
import module { specific_item }
```

## Module-Specific Features

### Hook Declarations

```daffyscript
on hook "hook-name" (payload: {field: type}) {
    // handler body
}
```

### Expect Hook

```daffyscript
expect hook "hook-name"
```

### Exports

```daffyscript
exports {
    function1,
    function2,
    on hook "hook-name",
}
```

## Program-Specific Features

### Command Declaration

```daffyscript
command "/command-name" (arg1: str, arg2: int) {
    // command handler
}
```

### Intercept Declaration

```daffyscript
intercept "pattern" (message: str) {
    // intercept handler
}
```

### Scheduled Execution

```daffyscript
every 5 minutes {
    // runs every 5 minutes
}

every 1 hour {
    // runs every hour
}

at "0 9 * * *" timezone "UTC" {
    // runs at 9 AM UTC daily (cron syntax)
}
```

### Event Handlers

```daffyscript
on event "event-name" (payload: {field: type}) {
    // event handler
}
```

## Recipe-Specific Features

### Room Configuration

```daffyscript
room {
    max_users: 25,
    voice: enabled,
    history: 30.days,
    language: "en",
    moderation: moderate,
}
```

### Service Configuration

```daffyscript
service "service-name" {
    from: "path/to/service.dssl",
    autostart: true,
}

service "service-name" {
    from: "path/to/service.dssl",
    config {
        key: "value",
    }
}
```

### Roles Configuration

```daffyscript
roles {
    role "admin" {
        can: [kick, mute, delete_message, pin_message]
    }
    role "member" {
        can: [send_message, react]
    }
    default_role: "member"
}
```

### Webhooks Configuration

```daffyscript
webhooks {
    on event "event-name" post to "https://example.com/webhook"
    on event "other-event" post to "https://example.com/other" headers {
        Authorization: "Bearer token"
    }
}
```

### Initialization

```daffyscript
on init {
    // runs when room is created
}
```

## Type Annotations

Type annotations can be used in:

- Variable declarations: `let x: int = 42`
- Function parameters: `fn f(x: int, y: str)`
- Function return types: `fn f() -> int`
- Struct fields: `struct S { x: int }`

## Scoping Rules

- Variables are block-scoped
- Functions are file-scoped
- Imports are file-scoped
- Public functions are exported from the module

## Operator Precedence

From highest to lowest:

1. Field access (`.`), indexing (`[]`), function calls
2. Unary operators (`-`, `not`)
3. Multiplicative (`*`, `/`, `%`)
4. Additive (`+`, `-`)
5. Comparison (`<`, `>`, `<=`, `>=`)
6. Equality (`==`, `!=`)
7. Logical AND (`and`)
8. Logical OR (`or`)
9. Null coalescing (`??`)
10. Assignment (`=`)
