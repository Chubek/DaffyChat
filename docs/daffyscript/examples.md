# Daffyscript Examples

Practical examples demonstrating common Daffyscript patterns.

## Hello World Module

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

## Message Counter Module

```daffyscript
module message_counter
version 1.0.0
description "Counts messages per user"

struct UserStats {
    user_id: str,
    count: int,
}

let mut message_counts: {str: int} = {}

pub fn increment_count(user_id: str) {
    let current = message_counts[user_id] ?? 0
    message_counts[user_id] = current + 1
}

pub fn get_count(user_id: str) -> int {
    return message_counts[user_id] ?? 0
}

on hook "message:sent" (payload: {author_id: str, text: str}) {
    increment_count(payload.author_id)
    
    let count = get_count(payload.author_id)
    if count % 10 == 0 {
        emit "ui:notification" {
            message: "User sent " + count + " messages!",
        }
    }
}

exports {
    get_count,
}
```

## Standup Bot Program

```daffyscript
program standup_bot
version 1.0.0
description "Daily standup automation"

import ldc.message
import ldc.storage

struct Update {
    author: str,
    text: str,
    timestamp: str,
}

fn start_standup() {
    ldc.storage.set("standup:active", "true")
    ldc.storage.set("standup:count", "0")
    ldc.message.send("🎯 Daily standup started! Use /update to share your progress.")
}

fn end_standup() {
    let count = ldc.storage.get("standup:count") ?? "0"
    ldc.storage.set("standup:active", "false")
    ldc.message.send("✅ Standup complete! " + count + " updates received.")
}

command "/standup" () {
    start_standup()
}

command "/update" (text: str) {
    let active = ldc.storage.get("standup:active") ?? "false"
    if active == "true" {
        let count = ldc.storage.get("standup:count") ?? "0"
        ldc.storage.set("standup:count", count + 1)
        ldc.message.send("✓ Update recorded")
    } else {
        ldc.message.send("No active standup. Use /standup to start.")
    }
}

command "/standup-end" () {
    end_standup()
}

every 1 day {
    start_standup()
}
```

## Auto-Moderator Program

```daffyscript
program auto_moderator
version 1.0.0
description "Automatic message moderation"

import ldc.message
import ldc.event

let banned_words: [str] = ["spam", "badword"]

fn contains_banned_word(text: str) -> bool {
    for word in banned_words {
        if text.contains(word) {
            return true
        }
    }
    return false
}

intercept "message" (message: str) {
    if contains_banned_word(message) {
        emit "moderation:flagged" {
            reason: "Banned word detected",
            message: message,
        }
    }
}

on event "message:sent" (payload: {message_id: str, text: str, author: str}) {
    if contains_banned_word(payload.text) {
        ldc.message.delete(payload.message_id)
        ldc.message.send("⚠️ Message removed: contains prohibited content")
    }
}
```

## Incident Response Recipe

```daffyscript
recipe "incident-response"
version 1.0.0
author "DevOps Team"
description "Emergency incident coordination room"

room {
    max_users: 50,
    voice: enabled,
    history: 7.days,
    language: "en",
    moderation: strict,
}

service "incident_tracker" {
    from: "services/incident_tracker.dssl",
    autostart: true,
}

program "incident_bot" {
    from: "programs/incident_bot.dfyp"
}

module "status_board" {
    from: "modules/status_board.dfy"
}

roles {
    role "incident-commander" {
        can: [kick, mute, delete_message, pin_message, manage_roles]
    }
    role "responder" {
        can: [send_message, react, upload_file, pin_message]
    }
    role "observer" {
        can: [send_message]
    }
    default_role: "observer"
}

webhooks {
    on event "incident.opened" post to "https://pagerduty.example.com/webhook"
    on event "incident.resolved" post to "https://pagerduty.example.com/webhook"
}

on init {
    ldc.message.send("🚨 Incident Response Room Active")
    ldc.message.send("Use /incident to track issues")
}
```

## Emoji Reactor Module

```daffyscript
module emoji_reactor
version 1.0.0
description "Auto-reacts to messages with emojis"

let emoji_map: {str: str} = {
    "thanks": "🙏",
    "good": "👍",
    "bad": "👎",
    "love": "❤️",
}

fn find_emoji(text: str) -> str? {
    for keyword, emoji in emoji_map {
        if text.contains(keyword) {
            return emoji
        }
    }
    return none
}

on hook "message:sent" (payload: {message_id: str, text: str}) {
    let emoji = find_emoji(payload.text)
    if emoji != none {
        emit "message:react" {
            message_id: payload.message_id,
            emoji: emoji,
        }
    }
}

exports {}
```

## Reminder Bot Program

```daffyscript
program reminder_bot
version 1.0.0
description "Set and manage reminders"

import ldc.message
import ldc.storage

command "/remind" (minutes: int, text: str) {
    let reminder_id = "reminder:" + minutes
    ldc.storage.set(reminder_id, text)
    ldc.message.send("⏰ Reminder set for " + minutes + " minutes")
}

every 1 minute {
    // Check for due reminders
    let reminder = ldc.storage.get("reminder:1")
    if reminder != none {
        ldc.message.send("⏰ Reminder: " + reminder)
        ldc.storage.delete("reminder:1")
    }
}
```

## Analytics Dashboard Module

```daffyscript
module analytics_dashboard
version 1.0.0
description "Real-time room analytics"

struct RoomStats {
    total_messages: int,
    active_users: int,
    peak_hour: int,
}

let mut stats: RoomStats = RoomStats {
    total_messages: 0,
    active_users: 0,
    peak_hour: 0,
}

pub fn update_stats() {
    emit "analytics:update" {
        total_messages: stats.total_messages,
        active_users: stats.active_users,
        peak_hour: stats.peak_hour,
    }
}

on hook "message:sent" (payload: {author_id: str}) {
    stats.total_messages = stats.total_messages + 1
    update_stats()
}

on hook "room:participant-joined" (payload: {peer_id: str}) {
    stats.active_users = stats.active_users + 1
    update_stats()
}

on hook "room:participant-left" (payload: {peer_id: str}) {
    stats.active_users = stats.active_users - 1
    update_stats()
}

exports {
    update_stats,
}
```

## More Examples

See the `stdext/daffyscript/` directory for additional examples:

- `frontend/participant_status/` - Participant status overlay
- `frontend/message_heatmap/` - Message activity heatmap
- `programs/standup_helper/` - Standup automation
- `recipes/incident_bridge/` - Incident response room
