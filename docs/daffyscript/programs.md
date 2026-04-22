# Daffyscript Programs

Programs are server-side room automation scripts that run as bots. They handle commands, intercept messages, schedule tasks, and respond to room events.

## Program Declaration

```daffyscript
program program_name
version 1.0.0
author "Your Name"
description "What this program does"
```

## Program Features

### Command Handlers

Respond to slash commands:

```daffyscript
command "/greet" (name: str) {
    ldc.message.send("Hello, " + name + "!")
}

command "/status" () {
    emit "status:requested" {}
}
```

### Message Interceptors

Process messages before they're delivered:

```daffyscript
intercept "pattern" (message: str) {
    // Modify or filter messages
    let filtered = process(message)
}
```

### Scheduled Tasks

Run code on a schedule:

```daffyscript
every 5 minutes {
    check_status()
}

every 1 hour {
    send_reminder()
}

at "0 9 * * *" timezone "UTC" {
    // Runs at 9 AM UTC daily (cron syntax)
    send_daily_report()
}
```

### Event Handlers

Respond to room events:

```daffyscript
on event "room:participant-joined" (payload: {peer_id: str, name: str}) {
    ldc.message.send("Welcome, " + payload.name + "!")
}

on event "message:sent" (payload: {text: str, author: str}) {
    process_message(payload.text)
}
```

## Complete Example

```daffyscript
program standup_helper
version 1.0.0
author "DaffyChat Team"
description "Helps teams run daily standups"

import ldc.event
import ldc.message
import ldc.storage

struct UpdateEntry {
    author: str,
    text: str,
}

struct StandupState {
    question: str,
    entries: int,
}

fn initialize_standup(question: str) {
    ldc.storage.set("standup:question", question)
    ldc.storage.set("standup:entries", "0")
    ldc.message.send("Standup started: " + question)
}

fn record_update(author: str, text: str) {
    let count = ldc.storage.get("standup:entries")
    ldc.storage.set("standup:entries", count + 1)
    emit "standup:update" {
        author: author,
        text: text,
    }
}

fn publish_digest(count: int) {
    ldc.message.send("Standup complete. " + count + " updates received.")
}

command "/standup" (question: str) {
    initialize_standup(question)
}

command "/update" (text: str) {
    record_update(ldc.user.name(), text)
}

command "/standup-end" () {
    let count = ldc.storage.get("standup:entries")
    publish_digest(count)
}

every 1 day {
    initialize_standup("What did you work on today?")
}
```

## Standard Library (ldc)

Programs have access to the `ldc` (Local DaffyChat) standard library:

### ldc.message

```daffyscript
ldc.message.send(text: str)
ldc.message.reply(message_id: str, text: str)
ldc.message.delete(message_id: str)
```

### ldc.storage

```daffyscript
ldc.storage.get(key: str) -> str?
ldc.storage.set(key: str, value: str)
ldc.storage.delete(key: str)
```

### ldc.event

```daffyscript
ldc.event.emit(name: str, payload: {})
ldc.event.subscribe(name: str)
```

### ldc.user

```daffyscript
ldc.user.name() -> str
ldc.user.id() -> str
```

### ldc.room

```daffyscript
ldc.room.id() -> str
ldc.room.participants() -> [str]
```

## Best Practices

- Keep command handlers simple and fast
- Use storage for persistent state
- Emit events for important actions
- Handle errors gracefully
- Document commands in description
