# DaffyChat Room Management Features

This document describes the new room management features implemented for DaffyChat.

## Overview

DaffyChat now supports custom room names, password protection, and enhanced room management capabilities.

## Features

### 1. Custom Room Names

Users can now create rooms with custom short names (4-10 characters, alphanumeric and dash only).

**Benefits:**
- Easy to remember and share
- Short URLs: `daffychat.ir/my-room` instead of `daffychat.ir/room.html?room=room_abc123xyz`
- Professional appearance

**Validation:**
- Length: 4-10 characters
- Allowed characters: a-z, A-Z, 0-9, dash (-)
- Unique across all active rooms

### 2. Password Protection

Rooms can be protected with passwords using SHA-512 hashing.

**Implementation:**
- Passwords are hashed using SHA-512 (via `third_party/sha2`)
- JWT tokens are issued upon successful authentication (via `third_party/jwt-cpp`)
- Tokens are stored in browser localStorage
- Token expiry: 1 hour (configurable)

**User Flow:**
1. User attempts to join password-protected room
2. Password prompt dialog appears
3. Password is hashed and sent to backend
4. Backend verifies hash and issues JWT token
5. Token is used for subsequent API calls

### 3. Room Creator Privileges

The user who creates a room becomes the administrator with special privileges:

**Admin Capabilities:**
- Delete the room
- View room statistics
- Manage participants (future feature)

### 4. Dead Room Cleanup

Rooms with no activity are automatically detected and can be cleaned up.

**Dead Room Criteria:**
- No participants
- No activity for > 1 hour (configurable)
- State is not Active

**Behavior:**
- When a user tries to create a room with an existing name
- System checks if the existing room is dead
- If dead, the old room is deleted and the name is reused
- If active, user is prompted to choose a different name

### 5. Name Suggestions

When a room name is taken, the system suggests alternatives:

**Using Natural.js:**
- Finds similar words based on string similarity
- Suggests variations of the attempted name

**Using Chance.js:**
- Generates random room names
- Ensures generated names meet validation criteria

### 6. Chat History Export

Users can download their chat history as JSON.

**Export Format:**
```json
{
  "room": {
    "name": "my-room",
    "url": "https://daffychat.ir/my-room"
  },
  "exported_at": "2025-05-20T15:30:00Z",
  "message_count": 42,
  "messages": [
    {
      "timestamp": "2025-05-20T15:25:00Z",
      "author": "user-abc123",
      "text": "Hello world!",
      "type": "message"
    }
  ]
}
```

### 7. Voice Activity Detection

Using Hark.js for automatic voice activity detection:

**Features:**
- Detects when users are speaking
- Visual indicator for active speakers
- Automatic background noise suppression during silence
- Configurable sensitivity threshold

**Implementation:**
- Uses Web Audio API
- Real-time speech detection
- Integrates with Tone.js for audio processing

### 8. Enhanced UI

**Fonts:**
- Primary: Inter (sans-serif)
- Monospace: JetBrains Mono (for code, room names, IDs)

**Icons:**
- Migrated from Boxicons to Lucide icons
- Consistent, modern icon set
- Better accessibility

**Code Editor:**
- Shiki for syntax highlighting
- Supports multiple languages
- Theme-aware (light/dark mode)

## Backend Architecture

### Database Layer

**ZethaRDB (Relational Database):**
- Stores room records with all metadata
- Base64 encoding for safe serialization
- Persistent storage on disk

**ZethaINDEX (String Index):**
- Fast lookup of room names
- O(1) duplicate detection
- Efficient name search

### Room Model

```cpp
struct Room {
  std::string id;                    // Generated ID (room_xxx)
  std::string display_name;          // Human-readable name
  std::string custom_name;           // Short URL name (4-10 chars)
  std::string password_hash;         // SHA-512 hash (empty if no password)
  std::string creator_id;            // Participant ID of creator
  RoomState state;                   // Active, Closing, Closed
  std::string created_at;            // ISO 8601 timestamp
  std::string last_activity_at;      // For dead room detection
  bool is_password_protected;        // Quick check flag
  std::vector<Participant> participants;
  std::vector<PeerSession> sessions;
};
```

### API Endpoints

**POST /api/rooms/create**
```json
{
  "custom_name": "my-room",
  "display_name": "My Awesome Room",
  "password": "optional-password"
}
```

**POST /api/rooms/authenticate**
```json
{
  "custom_name": "my-room",
  "password": "room-password"
}
```

**DELETE /api/rooms/{room_id}**
```json
{
  "room_id": "room_abc123",
  "requester_id": "participant_xyz789"
}
```

**GET /api/rooms/{custom_name}**
Returns room information (without sensitive data)

**GET /api/rooms/suggest?name={attempted_name}**
Returns name suggestions when a name is taken

## Frontend Components

### Room Creation Page (`create-room.html`)

- Real-time name validation
- Password protection toggle
- Name suggestions on conflict
- Random name generation

### Room Authentication (`app/components/room-auth.js`)

- Modal password prompt
- JWT token management
- Automatic retry on failure

### Chat History Export (`app/components/chat-history.js`)

- Tracks all messages
- JSON export with metadata
- One-click download

## URL Routing

### Short URLs

Users can access rooms via:
- `daffychat.ir/my-room`
- `daffychat.ir/r/my-room`

### Configuration

**Apache (.htaccess):**
```apache
RewriteRule ^([a-zA-Z0-9-]{4,10})$ r.html?name=$1 [L,QSA]
```

**Nginx:**
```nginx
location ~ ^/([a-zA-Z0-9-]{4,10})$ {
    if (!-e $request_filename) {
        rewrite ^/(.*)$ /r.html?name=$1 last;
    }
}
```

## Security Considerations

1. **Password Hashing:** SHA-512 with proper salt (implement salt in production)
2. **JWT Tokens:** HS256 algorithm, 1-hour expiry
3. **Input Validation:** Strict regex for room names
4. **SQL Injection:** Using ZethaDB's safe API (no raw SQL)
5. **XSS Protection:** All user input is sanitized before display

## Future Enhancements

1. Room capacity limits
2. Participant kick/ban functionality
3. Room moderation tools
4. Persistent chat history (optional)
5. Room analytics and statistics
6. Custom room themes
7. Scheduled room deletion
8. Room invitations via email

## Testing

### Manual Testing Checklist

- [ ] Create room with valid name
- [ ] Create room with invalid name (too short, too long, special chars)
- [ ] Create room with duplicate name (active room)
- [ ] Create room with duplicate name (dead room)
- [ ] Create password-protected room
- [ ] Join password-protected room with correct password
- [ ] Join password-protected room with wrong password
- [ ] Delete room as creator
- [ ] Attempt to delete room as non-creator
- [ ] Download chat history
- [ ] Access room via short URL
- [ ] Voice activity detection
- [ ] Icon rendering (Lucide)
- [ ] Font rendering (Inter, JetBrains Mono)

## Dependencies

### Backend
- `third_party/ZethaDB` - Database layer
- `third_party/sha2` - Password hashing
- `third_party/jwt-cpp` - JWT authentication

### Frontend
- `lib/lucide.js` - Icon library
- `lib/chance.min.js` - Random name generation
- `lib/natural.min.js` - String similarity
- `lib/Tone.min.js` - Audio processing
- `lib/hark.bundle.js` - Voice activity detection
- `lib/shiki.js` - Code syntax highlighting
- `fonts/inter.css` - Primary font
- `fonts/jetbraints-mono.css` - Monospace font

## Migration Guide

### Existing Rooms

Existing rooms with long IDs will continue to work. To migrate:

1. Admin assigns custom names to existing rooms
2. Old URLs redirect to new short URLs
3. Database migration script (to be created)

### Configuration

Update `config.json` with:
```json
{
  "rooms": {
    "name_min_length": 4,
    "name_max_length": 10,
    "dead_room_threshold_seconds": 3600,
    "jwt_secret": "change-this-in-production",
    "jwt_expiry_seconds": 3600
  }
}
```

## Troubleshooting

### Room name validation fails
- Check length (4-10 chars)
- Ensure only alphanumeric and dash
- Verify name is not taken

### Password authentication fails
- Verify password is correct
- Check JWT token expiry
- Clear localStorage and retry

### Voice detection not working
- Check microphone permissions
- Verify Hark.js is loaded
- Check browser compatibility

### Icons not displaying
- Verify Lucide.js is loaded
- Check `lucide.createIcons()` is called
- Inspect console for errors

## License

Same as DaffyChat main project.
