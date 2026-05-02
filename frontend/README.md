# DaffyChat Frontend

Web-based frontend for DaffyChat with Socket.IO voice transport support.

## Features

- 🎙️ **Voice Communication** - WebRTC-based voice chat with Socket.IO signaling
- 💬 **Text Chat** - Real-time messaging with markdown support
- 🎨 **Modern UI** - Clean, responsive design with dark mode
- 🔌 **Extensible** - Plugin system for custom features
- 📱 **Mobile Friendly** - Works on desktop and mobile browsers

## Quick Start

### Development Server

```bash
# Serve frontend files
cd frontend
python3 -m http.server 8000
```

Then open http://localhost:8000 in your browser.

### Voice Demo

1. Start the DaffyChat signaling server with Socket.IO:
   ```bash
   ./daffy-signaling --serve-socketio
   ```

2. Open http://localhost:8000/voice-demo.html

3. Click "Connect" → "Join Room" → "Start Call"

## Directory Structure

```
frontend/
├── app/
│   ├── api/              # API clients and transport layers
│   │   └── socketio-voice-transport.js  # Socket.IO voice client
│   ├── components/       # Reusable UI components
│   ├── hooks/           # Custom hooks
│   ├── state/           # State management
│   └── styles/          # CSS styles
├── lib/                 # Third-party libraries
│   ├── socket.io.min.js # Socket.IO client
│   ├── alpine.js        # Alpine.js framework
│   └── ...
├── resources/           # Static assets
├── extensions/          # Frontend extensions
├── index.html          # Landing page
├── room.html           # Chat room interface
├── voice-demo.html     # Voice transport demo
└── README.md           # This file
```

## Socket.IO Voice Transport

### Basic Usage

```javascript
// Create transport instance
const transport = new SocketIOVoiceTransport({
  serverUrl: 'http://localhost:7002',
  peerId: 'user-123',
  room: 'my-room',
  debug: true
});

// Connect and join
await transport.connect();
await transport.joinRoom('my-room');
await transport.startCall();

// Handle remote audio
transport.on('remoteStream', (stream) => {
  audioElement.srcObject = stream;
});
```

### Configuration

```javascript
const config = {
  serverUrl: 'http://localhost:7002',  // Socket.IO server URL
  peerId: 'unique-id',                 // Your peer ID
  room: 'room-name',                   // Room to join
  iceServers: [                        // STUN/TURN servers
    { urls: 'stun:stun.l.google.com:19302' }
  ],
  debug: false                         // Enable debug logging
};
```

### Events

- `connected` - Socket.IO connection established
- `disconnected` - Socket.IO connection lost
- `remoteStream` - Remote audio stream received
- `error` - Error occurred
- `peerReady` - Peer is ready for connection
- `callStarted` - Voice call started
- `callEnded` - Voice call ended

## Integration with Existing Pages

### Add to room.html

```html
<!-- Add Socket.IO library -->
<script src="lib/socket.io.min.js"></script>
<script src="app/api/socketio-voice-transport.js"></script>

<script>
  // Initialize voice transport
  const voiceTransport = new SocketIOVoiceTransport({
    serverUrl: window.location.protocol + '//' + window.location.hostname + ':7002',
    peerId: getCurrentUserId(),
    room: getCurrentRoomName()
  });

  // Connect when joining room
  async function joinVoiceRoom() {
    await voiceTransport.connect();
    await voiceTransport.joinRoom(getCurrentRoomName());
    await voiceTransport.startCall();
  }

  // Handle remote audio
  voiceTransport.on('remoteStream', (stream) => {
    const audio = document.getElementById('remote-audio');
    audio.srcObject = stream;
  });
</script>
```

## Browser Requirements

- Chrome 56+ / Edge 79+
- Firefox 52+
- Safari 11+
- Opera 43+

**Required APIs:**
- WebRTC (RTCPeerConnection)
- getUserMedia
- WebSocket or long-polling

## Development

### File Serving

For development, any static file server works:

```bash
# Python
python3 -m http.server 8000

# Node.js
npx http-server -p 8000

# PHP
php -S localhost:8000
```

### Testing Voice

1. Open two browser windows
2. Connect both to the same room
3. Grant microphone permissions
4. Start call in both windows
5. You should hear audio from the other window

### Debugging

Enable debug mode in the transport:

```javascript
const transport = new SocketIOVoiceTransport({
  debug: true  // Logs all events to console
});
```

Check browser console for:
- Socket.IO connection status
- WebRTC peer connection state
- ICE candidate gathering
- Audio stream events

## Production Deployment

### Build for Production

```bash
# Minify JavaScript (optional)
npm install -g terser
terser app/api/socketio-voice-transport.js -o app/api/socketio-voice-transport.min.js -c -m

# Optimize images
npm install -g imagemin-cli
imagemin resources/*.png --out-dir=resources/
```

### Nginx Configuration

```nginx
server {
    listen 443 ssl http2;
    server_name your-domain.com;

    root /usr/local/share/daffychat/frontend;
    index index.html;

    # Frontend routes
    location / {
        try_files $uri $uri/ /index.html;
    }

    # Socket.IO proxy
    location /socket.io/ {
        proxy_pass http://127.0.0.1:7002;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
    }
}
```

### Environment Configuration

Create `app/config.js`:

```javascript
const DAFFYCHAT_CONFIG = {
  production: true,
  signalingUrl: 'wss://your-domain.com/ws',
  socketIOUrl: 'https://your-domain.com',
  apiUrl: 'https://your-domain.com/api',
  stunServers: [
    'stun:stun.l.google.com:19302',
    'stun:stun1.l.google.com:19302'
  ],
  turnServer: {
    urls: 'turn:your-domain.com:3478',
    username: 'daffychat',
    credential: 'your-secret'
  }
};
```

## Troubleshooting

### No Audio

1. Check microphone permissions in browser
2. Verify STUN/TURN server configuration
3. Check browser console for WebRTC errors
4. Test with voice-demo.html

### Connection Issues

1. Verify Socket.IO server is running on port 7002
2. Check browser console for connection errors
3. Test direct connection: `curl http://localhost:7002/socket.io/`
4. Verify firewall allows WebSocket connections

### ICE Connection Failed

1. Add TURN server to configuration
2. Check NAT/firewall settings
3. Verify STUN servers are reachable
4. Review ICE candidate gathering in console

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for development guidelines.

## License

See [LICENSE](../LICENSE) file for details.

## Support

- Documentation: https://daffychat.readthedocs.io
- Issues: https://github.com/yourusername/daffychat/issues
- Demo: Open voice-demo.html in your browser
