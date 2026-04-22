# Daffyscript Recipes

Recipes are room configuration and assembly documents. They describe how services, programs, modules, roles, and webhooks are bundled into a reusable room profile.

## Recipe Declaration

```daffyscript
recipe "recipe-name"
version 1.0.0
author "Your Name"
description "What this recipe provides"
```

## Recipe Components

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

**Available options:**
- `max_users` - Maximum participants (integer)
- `voice` - Voice support (`enabled` or `disabled`)
- `history` - Message retention (e.g., `30.days`, `7.days`)
- `language` - Room language code (e.g., `"en"`, `"es"`)
- `moderation` - Moderation level (`strict`, `moderate`, `relaxed`)

### Service Configuration

```daffyscript
service "service-name" {
    from: "path/to/service.dssl",
    autostart: true,
}

service "analytics" {
    from: "../../dssl/room_analytics/room_analytics.dssl",
    autostart: true,
    config {
        retention: "90.days",
        sampling: "1.minute",
    }
}
```

### Program Integration

```daffyscript
program "bot-name" {
    from: "../programs/standup_helper/standup_helper.dfyp"
}
```

### Module Integration

```daffyscript
module "module-name" {
    from: "../frontend/message_heatmap/message_heatmap.dfy"
}
```

### Roles Configuration

```daffyscript
roles {
    role "admin" {
        can: [kick, mute, delete_message, pin_message, manage_roles]
    }
    role "moderator" {
        can: [mute, delete_message]
    }
    role "member" {
        can: [send_message, react, upload_file]
    }
    role "observer" {
        can: [send_message]
    }
    default_role: "member"
}
```

**Available permissions:**
- `kick` - Kick users from room
- `mute` - Mute users
- `delete_message` - Delete any message
- `pin_message` - Pin messages
- `manage_roles` - Assign roles
- `send_message` - Send messages
- `react` - React to messages
- `upload_file` - Upload files
- `manage_webhooks` - Configure webhooks

### Webhooks Configuration

```daffyscript
webhooks {
    on event "standup.started" post to "https://hooks.example.com/standup"
    
    on event "moderation.case.opened" post to "https://hooks.example.com/moderation" headers {
        Authorization: "Bearer secret-token",
        Content-Type: "application/json",
    }
}
```

### Conditional Configuration

```daffyscript
when "production" {
    service "monitoring" {
        from: "monitoring.dssl",
        autostart: true,
    }
}
```

### Initialization

```daffyscript
on init {
    ldc.message.send("Room initialized from recipe: incident-bridge")
    ldc.storage.set("recipe:version", "1.0.0")
}
```

## Complete Example

```daffyscript
recipe "incident-bridge"
version 1.0.0
author "DaffyChat Team"
description "Room profile for incident review with diagnostics, moderation, and automation"

room {
    max_users: 25,
    voice: enabled,
    history: 30.days,
    language: "en",
    moderation: moderate,
}

service "room_analytics" {
    from: "../../dssl/room_analytics/room_analytics.dssl",
    autostart: true,
}

service "moderation_assistant" {
    from: "../../dssl/moderation_assistant/moderation_assistant.dssl",
    autostart: true,
}

service "voice_ops" {
    from: "../../dssl/voice_ops/voice_ops.dssl",
    autostart: true,
}

program "standup_helper" {
    from: "../programs/standup_helper/standup_helper.dfyp"
}

module "message_heatmap" {
    from: "../frontend/message_heatmap/message_heatmap.dfy"
}

module "participant_status_overlay" {
    from: "../frontend/participant_status/participant_status.dfy"
}

roles {
    role "incident-lead" {
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
    on event "standup.started" post to "https://hooks.example.com/incident/standup"
    on event "moderation.case.opened" post to "https://hooks.example.com/incident/moderation"
}

on init {
    ldc.message.send("Incident Bridge room is live. Open diagnostics first, then coordinate remediation.")
}
```

## Using Recipes

### Creating a Room from Recipe

```bash
# Compile the recipe
daffyscript incident_bridge.dfyr

# Create room with recipe
daffychat create-room --recipe incident_bridge.wasm
```

### Saving Room as Recipe

```bash
# Export current room configuration
daffychat export-recipe room-123 > my_room.dfyr
```

## Best Practices

### Organize by Purpose

Group related components:

```daffyscript
-- Analytics services
service "room_analytics" { ... }
service "user_analytics" { ... }

-- Moderation tools
service "moderation_assistant" { ... }
program "auto_moderator" { ... }
```

### Use Descriptive Names

```daffyscript
recipe "incident-response-room"  // Good
recipe "room1"                   // Bad
```

### Document Dependencies

```daffyscript
-- Requires: room_analytics.dssl, standup_helper.dfyp
-- Optional: voice_ops.dssl
```

### Version Carefully

```daffyscript
version 1.0.0  // Initial release
version 1.1.0  // Added feature
version 2.0.0  // Breaking change
```

### Test Before Deployment

1. Compile recipe: `daffyscript recipe.dfyr`
2. Validate: `daffyscript --validate recipe.dfyr`
3. Test in dev room
4. Deploy to production

## Recipe Composition

Recipes can reference other recipes (future feature):

```daffyscript
include "base-room.dfyr"
include "analytics-addon.dfyr"
```

## Troubleshooting

**Recipe won't compile:**
- Check file paths are correct
- Verify all referenced files exist
- Ensure syntax is valid

**Services won't start:**
- Check service paths
- Verify autostart setting
- Check service dependencies

**Webhooks not firing:**
- Verify event names match
- Check webhook URL is accessible
- Validate headers format
