# Daffyscript Modules

Modules are frontend extensions that run in the browser as WebAssembly. They can respond to room events, modify UI state, and interact with the DaffyChat bridge.

## Module Declaration

Every module file (`.dfy`) starts with:

```daffyscript
module module_name
version 1.0.0
```

Optional metadata:

```daffyscript
author "Your Name"
description "What this module does"
```

## Module Structure

A typical module contains:

1. **Imports** - External dependencies
2. **Type definitions** - Structs and enums
3. **Functions** - Business logic
4. **Hook handlers** - Event responders
5. **Exports** - Public API

## Imports

```daffyscript
import dfc_bridge_sse
import other_module
```

## Hook System

Modules respond to events through hooks. Hooks are registered with the `on hook` declaration:

```daffyscript
on hook "hook-name" (payload: {field: type}) {
    // handler code
}
```

### Available Hooks

**Room hooks:**
- `room:created` - Room is created
- `room:destroyed` - Room is destroyed
- `room:participant-joined` - User joins room
- `room:participant-left` - User leaves room

**Message hooks:**
- `message:sent` - Message is sent
- `message:received` - Message is received
- `message:before-send` - Before message is sent (can modify)

**Voice hooks:**
- `voice:state-changed` - Voice state changes

**Extension hooks:**
- `extension:loaded` - Extension is loaded
- `extension:error` - Extension encounters error

### Hook Payload Types

Hook handlers receive typed payloads:

```daffyscript
on hook "room:participant-joined" (payload: {peer_id: str, name: str}) {
    // payload.peer_id and payload.name are available
}

on hook "voice:state-changed" (payload: {transport: str}) {
    // payload.transport is available
}
```

## Emitting Events

Modules can emit custom events that other modules or the frontend can observe:

```daffyscript
emit "custom-event" {
    field1: value1,
    field2: value2,
}
```

Standard events:

```daffyscript
emit "message:send" {
    text: "Hello, world!",
}

emit "ui:notification" {
    message: "Something happened",
    level: "info",
}
```

## Expecting Hooks

Declare which hooks your module expects to be available:

```daffyscript
expect hook "custom-hook"
expect hook "another-hook"
```

This documents the module's dependencies and allows the runtime to validate hook availability.

## Exports

Specify which functions and hooks are part of the module's public API:

```daffyscript
exports {
    public_function,
    another_function,
    on hook "hook-name",
}
```

Only exported items are accessible from outside the module.

## Complete Example

```daffyscript
module participant_status_overlay
version 1.0.0
author "DaffyChat Team"
description "Displays participant status badges"

import dfc_bridge_sse

struct Badge {
    peer_id: str,
    label: str,
    tone: str,
}

pub fn set_badge(peer_id: str, label: str, tone: str) {
    emit "stdext.participant-status.badge" {
        peer_id: peer_id,
        label: label,
        tone: tone,
    }
}

pub fn clear_badge(peer_id: str) {
    emit "stdext.participant-status.clear" {
        peer_id: peer_id,
    }
}

on hook "room:participant-joined" (payload: {peer_id: str, name: str}) {
    set_badge(payload.peer_id, "New", "green")
}

on hook "room:participant-left" (payload: {peer_id: str}) {
    clear_badge(payload.peer_id)
}

on hook "voice:state-changed" (payload: {transport: str}) {
    emit "stdext.participant-status.voice-banner" {
        transport: payload.transport,
        headline: "Voice state changed",
    }
}

expect hook "stdext.participant-status.badge"
expect hook "stdext.participant-status.clear"
expect hook "stdext.participant-status.voice-banner"

exports {
    set_badge,
    clear_badge,
    on hook "room:participant-joined",
    on hook "room:participant-left",
    on hook "voice:state-changed",
}
```

## Module Lifecycle

1. **Load** - Module WASM is loaded into the browser
2. **Initialize** - Module initialization code runs
3. **Register** - Hooks are registered with the bridge
4. **Active** - Module responds to events
5. **Unload** - Module is unloaded and hooks are removed

## Best Practices

### Keep Modules Focused

Each module should have a single, well-defined purpose:

```daffyscript
// Good: focused on one feature
module message_formatter
version 1.0.0

// Bad: trying to do too much
module everything_kitchen_sink
version 1.0.0
```

### Use Type Annotations

Always annotate hook payloads and function parameters:

```daffyscript
// Good
on hook "event" (payload: {id: str, count: int}) {
    // ...
}

// Bad
on hook "event" (payload) {
    // ...
}
```

### Document Expected Hooks

Use `expect hook` to document dependencies:

```daffyscript
expect hook "custom-hook"
expect hook "another-hook"
```

### Export Intentionally

Only export what's needed:

```daffyscript
// Private helper
fn internal_helper() {
    // ...
}

// Public API
pub fn public_function() {
    internal_helper()
}

exports {
    public_function,  // Only this is exported
}
```

### Handle Errors Gracefully

Use try-catch for operations that might fail:

```daffyscript
on hook "event" (payload: {data: str}) {
    try {
        let parsed = parse_json(payload.data)
        process(parsed)
    } catch err {
        emit "error" {
            message: "Failed to process event",
        }
    }
}
```

## Integration with Frontend

Modules integrate with the frontend through the DaffyChat bridge:

1. **Load the module** in the Extensions tab
2. **Hooks are registered** automatically
3. **Events flow** from backend → bridge → module
4. **Module emits** events back to the bridge
5. **Frontend reacts** to module events

## Debugging

Use emit statements for logging:

```daffyscript
emit "debug:log" {
    message: "Debug info",
    value: some_value,
}
```

The frontend console will show these events.

## Performance Considerations

- Keep hook handlers fast
- Avoid heavy computation in event handlers
- Use async patterns for I/O operations
- Batch UI updates when possible

## Security

Modules run in a sandboxed WASM environment:

- No direct DOM access
- No direct network access
- Limited to declared permissions
- Memory isolated from other modules

Declare required permissions in the module manifest (separate JSON file).
