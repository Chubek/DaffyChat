# DaffyChat Room Management - Quick Start Guide

## For Developers

### 1. Backend Setup

```bash
# Navigate to project root
cd /path/to/daffychat

# Ensure dependencies are present
ls third_party/ZethaDB/
ls third_party/sha2/
ls third_party/jwt-cpp/

# Create data directory for room database
mkdir -p data

# Build the project
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 2. Frontend Setup

```bash
# Navigate to frontend directory
cd frontend

# Verify all libraries are present
ls lib/lucide.js
ls lib/chance.min.js
ls lib/natural.min.js
ls lib/Tone.min.js
ls lib/hark.bundle.js
ls lib/shiki.js

# Verify fonts are present
ls fonts/inter.css
ls fonts/jetbraints-mono.css
```

### 3. Web Server Configuration

#### Option A: Apache
```bash
# Copy .htaccess to web root
cp frontend/.htaccess /var/www/html/

# Enable mod_rewrite
sudo a2enmod rewrite
sudo systemctl restart apache2
```

#### Option B: Nginx
```bash
# Add routing configuration to nginx.conf
sudo nano /etc/nginx/sites-available/daffychat

# Include the routing rules from nginx-routing.conf
# Reload nginx
sudo nginx -t
sudo systemctl reload nginx
```

### 4. Test the Implementation

#### Create a Room
1. Open browser to `http://localhost/create-room.html`
2. Enter room name: `test-room`
3. Optionally set password
4. Click "Create Room"

#### Join via Short URL
1. Navigate to `http://localhost/test-room`
2. Should redirect to room page
3. If password protected, enter password

#### Test Features
- Send messages (tracked in chat history)
- Click download button to export JSON
- If creator, test delete room button
- Test voice detection (grant mic permissions)

### 5. API Testing

```bash
# Create room
curl -X POST http://localhost/api/rooms/create \
  -H "Content-Type: application/json" \
  -d '{
    "custom_name": "api-test",
    "display_name": "API Test Room",
    "password": "secret123"
  }'

# Authenticate
curl -X POST http://localhost/api/rooms/authenticate \
  -H "Content-Type: application/json" \
  -d '{
    "custom_name": "api-test",
    "password": "secret123"
  }'

# Get room info
curl http://localhost/api/rooms/api-test

# Delete room (requires token)
curl -X DELETE http://localhost/api/rooms/{room_id} \
  -H "Authorization: Bearer {token}" \
  -H "Content-Type: application/json" \
  -d '{
    "room_id": "room_abc123",
    "requester_id": "participant_xyz789"
  }'
```

## For Users

### Creating a Room

1. Go to DaffyChat homepage
2. Click "Create a Room"
3. Choose a unique name (4-10 characters)
4. Optionally set a password
5. Click "Create Room"
6. Share the short URL: `daffychat.ir/your-room`

### Joining a Room

**Public Room:**
- Just visit `daffychat.ir/room-name`

**Password-Protected Room:**
1. Visit `daffychat.ir/room-name`
2. Enter password when prompted
3. Your access token is saved for 1 hour

### Room Features

**Chat:**
- Send text messages
- Use markdown formatting
- Code blocks with syntax highlighting

**Voice:**
- Automatic voice detection
- Speaking indicators
- Background noise suppression

**Management (Creator Only):**
- Download chat history as JSON
- Delete the room

### Keyboard Shortcuts

- `Enter` - Send message
- `Shift+Enter` - New line
- `/help` - Show commands
- `/clear` - Clear chat

## Troubleshooting

### Room name validation fails
**Problem:** "Invalid room name" error

**Solution:**
- Use 4-10 characters only
- Only letters, numbers, and dash (-)
- No spaces or special characters

### Password authentication fails
**Problem:** "Invalid password" error

**Solution:**
- Double-check password
- Clear browser cache and try again
- Check if token expired (1 hour)

### Icons not showing
**Problem:** Boxes instead of icons

**Solution:**
- Verify `lib/lucide.js` is loaded
- Check browser console for errors
- Ensure `lucide.createIcons()` is called

### Voice detection not working
**Problem:** No speaking indicators

**Solution:**
- Grant microphone permissions
- Check browser compatibility
- Verify `lib/hark.bundle.js` is loaded

### Short URLs not working
**Problem:** 404 error on `daffychat.ir/room-name`

**Solution:**
- Check web server configuration
- Verify .htaccess or nginx rules
- Test with `daffychat.ir/r.html?name=room-name`

## Configuration

### Production Settings

Edit `config.json`:

```json
{
  "rooms": {
    "name_min_length": 4,
    "name_max_length": 10,
    "dead_room_threshold_seconds": 3600,
    "jwt_secret": "CHANGE-THIS-TO-RANDOM-STRING",
    "jwt_expiry_seconds": 3600
  },
  "database": {
    "rooms_db_path": "/var/lib/daffychat/rooms.db"
  }
}
```

**Important:** Change `jwt_secret` to a random string in production!

### Security Checklist

- [ ] Change JWT secret
- [ ] Add password salt
- [ ] Enable HTTPS
- [ ] Configure CORS properly
- [ ] Set up rate limiting
- [ ] Enable security headers
- [ ] Regular database backups

## Support

For issues or questions:
1. Check ROOM_FEATURES.md for detailed documentation
2. Review IMPLEMENTATION_SUMMARY.md for technical details
3. Check browser console for errors
4. Verify all dependencies are installed

## License

Same as DaffyChat main project.
