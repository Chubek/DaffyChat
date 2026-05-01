# Enabling Frontend Voice Transport

This document explains how to enable the Socket.IO-based voice transport layer for non-native clients (web browsers, mobile apps) in DaffyChat.

## Overview

DaffyChat now supports voice communication for web-based frontends through a Socket.IO abstraction layer built on top of the existing Voice Transport API. This allows browser clients to establish WebRTC voice connections without requiring native WebSocket implementations.

## Architecture

The Socket.IO voice transport layer provides:

- **Real-time bidirectional communication** between frontend clients and the signaling server
- **Automatic reconnection** handling for unstable network connections
- **Event-based messaging** using Socket.IO's familiar event model
- **Compatibility** with the existing HTTP-based Voice Transport API

### Components

1. **SocketIOVoiceTransport** (`include/daffy/signaling/socketio_voice_transport.hpp`)
   - C++ server-side Socket.IO adapter
   - Manages client sessions and connection lifecycle
   - Bridges Socket.IO events to the signaling server

2. **SignalingServer Integration** (`src/cli/signaling_main.cpp`)
   - Runs Socket.IO transport alongside uWebSockets signaling
   - Configurable via command-line flags

3. **Frontend Socket.IO Client** (`frontend/lib/socket.io.min.js`)
   - Pre-bundled Socket.IO client library
   - Ready for integration into web applications

## Enabling Socket.IO Voice Transport

### Option 1: Using `--serve-socketio` Flag

Start the signaling server with Socket.IO voice transport enabled:

```bash
./daffy-signaling --serve-socketio
```

This will:
- Start the uWebSockets signaling server on the configured port (default: 7001)
- Start the Socket.IO voice transport server on port 7002
- Enable both native WebSocket and Socket.IO clients to connect

### Option 2: Using `--enable-socketio-voice` Flag

Enable Socket.IO voice transport with the standard `--serve` mode:

```bash
./daffy-signaling --serve --enable-socketio-voice
```

### Option 3: Using `daffy-backend` with Voice Peer

The `daffy-backend` executable can also enable Socket.IO voice transport when running in voice peer mode:

```bash
./daffy-backend --voice-peer --enable-socketio-voice
```

## Configuration

### Server Configuration

The Socket.IO voice transport uses the following default configuration:

- **Bind Address**: Same as signaling server (from config file)
- **Port**: 7002 (hardcoded, can be modified in `src/cli/signaling_main.cpp`)
- **Transport**: Socket.IO over HTTP/WebSocket

### Customizing the Port

To change the Socket.IO port, modify the following line in `src/cli/signaling_main.cpp`:

```cpp
socketio_config.port = 7002;  // Change to your desired port
```

Then rebuild the project:

```bash
cmake --build build
```

## Frontend Integration

### Step 1: Include Socket.IO Client

The Socket.IO client library is already bundled in `frontend/lib/socket.io.min.js`. Include it in your HTML:

```html
<script src="/lib/socket.io.min.js"></script>
```

### Step 2: Connect to Socket.IO Voice Transport

```javascript
// Connect to the Socket.IO voice transport server
const socket = io('http://localhost:7002', {
  transports: ['websocket', 'polling']
});

// Handle connection
socket.on('connect', () => {
  console.log('Connected to Socket.IO voice transport');
  
  // Send peer_id to establish session
  socket.emit('connect', { peer_id: 'web-client-123' });
});

// Handle connection confirmation
socket.on('connected', (data) => {
  console.log('Session established:', data);
  // data contains: { connection_id, peer_id }
});

// Handle incoming signaling messages
socket.on('signal', (message) => {
  const msg = JSON.parse(message);
  console.log('Received signal:', msg);
  
  // Handle different message types
  switch (msg.type) {
    case 'join-ack':
      handleJoinAck(msg);
      break;
    case 'peer-ready':
      handlePeerReady(msg);
      break;
    case 'offer':
      handleOffer(msg);
      break;
    case 'answer':
      handleAnswer(msg);
      break;
    case 'ice-candidate':
      handleIceCandidate(msg);
      break;
  }
});

// Send signaling messages
function sendSignal(message) {
  socket.emit('signal', JSON.stringify(message));
}

// Example: Join a room
sendSignal({
  type: 'join',
  room: 'alpha',
  peer_id: 'web-client-123'
});
```

### Step 3: Integrate with WebRTC

```javascript
// Create WebRTC peer connection
const peerConnection = new RTCPeerConnection({
  iceServers: [
    { urls: 'stun:stun.l.google.com:19302' }
  ]
});

// Handle ICE candidates
peerConnection.onicecandidate = (event) => {
  if (event.candidate) {
    sendSignal({
      type: 'ice-candidate',
      room: 'alpha',
      peer_id: 'web-client-123',
      candidate: event.candidate.candidate,
      sdpMid: event.candidate.sdpMid,
      sdpMLineIndex: event.candidate.sdpMLineIndex
    });
  }
};

// Handle incoming audio stream
peerConnection.ontrack = (event) => {
  const audioElement = document.getElementById('remote-audio');
  audioElement.srcObject = event.streams[0];
};

// Get local audio stream
navigator.mediaDevices.getUserMedia({ audio: true, video: false })
  .then((stream) => {
    stream.getTracks().forEach((track) => {
      peerConnection.addTrack(track, stream);
    });
  });
```

## Event Reference

### Client → Server Events

#### `connect`
Establish a new session with the voice transport server.

**Payload:**
```json
{
  "peer_id": "string"
}
```

#### `signal`
Send a signaling message (join, offer, answer, ICE candidate).

**Payload:** JSON string containing the signaling message

### Server → Client Events

#### `connected`
Confirmation that the session has been established.

**Payload:**
```json
{
  "connection_id": "string",
  "peer_id": "string"
}
```

#### `signal`
Incoming signaling message from the server.

**Payload:** JSON string containing the signaling message

## Message Types

The Socket.IO voice transport supports all standard DaffyChat signaling message types:

- `join` - Join a voice room
- `join-ack` - Acknowledgment of room join
- `leave` - Leave a voice room
- `peer-ready` - Notification that a peer is ready
- `peer-left` - Notification that a peer has left
- `offer` - WebRTC offer
- `answer` - WebRTC answer
- `ice-candidate` - ICE candidate for connection establishment
- `error` - Error message

## Troubleshooting

### Socket.IO Server Not Starting

**Problem:** Socket.IO voice transport fails to start

**Solution:**
- Check that port 7002 is not already in use
- Verify that the signaling server is running
- Check logs for error messages

### Frontend Cannot Connect

**Problem:** Browser client cannot connect to Socket.IO server

**Solution:**
- Verify the Socket.IO server URL is correct
- Check browser console for connection errors
- Ensure CORS is properly configured if accessing from a different origin
- Try using polling transport as fallback: `transports: ['polling', 'websocket']`

### No Audio Stream

**Problem:** WebRTC connection established but no audio

**Solution:**
- Check browser permissions for microphone access
- Verify ICE candidates are being exchanged
- Check that STUN/TURN servers are configured correctly
- Inspect WebRTC connection state in browser DevTools

### Messages Not Being Received

**Problem:** Signaling messages are sent but not received

**Solution:**
- Verify the `peer_id` matches on both ends
- Check that the room name is correct
- Ensure the signaling server is processing messages (check logs)
- Verify the Socket.IO event handlers are properly registered

## Performance Considerations

### Polling vs WebSocket

Socket.IO supports multiple transports:

- **WebSocket**: Lower latency, recommended for voice
- **Polling**: Fallback for restrictive networks

Configure transport priority:

```javascript
const socket = io('http://localhost:7002', {
  transports: ['websocket', 'polling']  // Try WebSocket first
});
```

### Event Loop Polling

The Socket.IO voice transport polls for events every 100ms. This can be adjusted in `src/signaling/socketio_voice_transport.cpp`:

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Adjust polling interval
```

## Security Considerations

### Production Deployment

For production use, consider:

1. **Enable TLS/SSL** for Socket.IO connections
2. **Add authentication** to verify client identity
3. **Implement rate limiting** to prevent abuse
4. **Validate all incoming messages** before processing
5. **Use secure WebSocket (wss://)** instead of ws://

### Authentication Example

```javascript
const socket = io('https://your-server.com:7002', {
  auth: {
    token: 'your-jwt-token'
  }
});
```

## Advanced Usage

### Custom Event Handlers

You can extend the Socket.IO voice transport with custom events:

```cpp
// In src/signaling/socketio_voice_transport.cpp
void SocketIOVoiceTransport::HandleCustomEvent(const std::string& socket_id, const std::string& data) {
  // Custom event handling logic
}
```

### Multiple Rooms

Clients can join multiple rooms by sending multiple `join` messages:

```javascript
sendSignal({ type: 'join', room: 'alpha', peer_id: 'client-1' });
sendSignal({ type: 'join', room: 'beta', peer_id: 'client-1' });
```

## Testing

### Manual Testing

1. Start the signaling server with Socket.IO:
   ```bash
   ./daffy-signaling --serve-socketio
   ```

2. Open the browser console and test connection:
   ```javascript
   const socket = io('http://localhost:7002');
   socket.on('connect', () => console.log('Connected!'));
   ```

3. Send a test message:
   ```javascript
   socket.emit('connect', { peer_id: 'test-client' });
   ```

### Integration Testing

Run the existing signaling integration tests to verify compatibility:

```bash
ctest --test-dir build -R signaling
```

## Future Enhancements

Planned improvements for the Socket.IO voice transport:

- [ ] Server-Sent Events (SSE) support for one-way streaming
- [ ] Binary message support for reduced bandwidth
- [ ] Automatic reconnection with session recovery
- [ ] Room-based broadcasting for multi-party calls
- [ ] Metrics and monitoring endpoints
- [ ] WebSocket compression support

## References

- [Voice Transport API Documentation](VOICE_TRANSPORT_API.md)
- [Socket.IO Documentation](https://socket.io/docs/)
- [WebRTC API](https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API)
- [DaffyChat Configuration Guide](docs/configuration.md)

## Support

For issues or questions:

1. Check the troubleshooting section above
2. Review the signaling server logs
3. Inspect browser console for client-side errors
4. Consult the Voice Transport API documentation
