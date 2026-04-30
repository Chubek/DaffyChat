(function bootstrapBridge(global) {
  const hooks = new Map();
  const eventLog = [];
  const MAX_LOG_SIZE = 200;
  let eventCounter = 0;
  let streamCounter = 0;
  const DEFAULTS = {
    signalingAdminUrl: '',
    voiceDiagnosticsUrl: 'http://127.0.0.1:7000/bridge'
  };
  const bridgeStream = {
    source: null,
    url: '',
    connected: false,
    lastError: '',
    reconnects: 0
  };

  function readBootstrapConfig() {
    const params = new URLSearchParams(global.location && global.location.search ? global.location.search : '');
    const signalingAdminUrl =
      params.get('signaling_admin') ||
      global.localStorage.getItem('daffy-signaling-admin-url') ||
      DEFAULTS.signalingAdminUrl;
    const voiceDiagnosticsUrl =
      params.get('voice_admin') ||
      global.localStorage.getItem('daffy-voice-admin-url') ||
      DEFAULTS.voiceDiagnosticsUrl;
    return {
      signalingAdminUrl,
      voiceDiagnosticsUrl
    };
  }

  function persistBootstrapConfig(config) {
    if (!config) return readBootstrapConfig();
    if (typeof config.signalingAdminUrl === 'string') {
      if (config.signalingAdminUrl) {
        global.localStorage.setItem('daffy-signaling-admin-url', config.signalingAdminUrl);
      } else {
        global.localStorage.removeItem('daffy-signaling-admin-url');
      }
    }
    if (typeof config.voiceDiagnosticsUrl === 'string') {
      if (config.voiceDiagnosticsUrl) {
        global.localStorage.setItem('daffy-voice-admin-url', config.voiceDiagnosticsUrl);
      } else {
        global.localStorage.removeItem('daffy-voice-admin-url');
      }
    }
    return readBootstrapConfig();
  }

  function registerHook(name, handler) {
    hooks.set(name, handler);
  }

  function removeHook(name) {
    hooks.delete(name);
  }

  function emit(name, payload) {
    eventCounter++;
    const event = {
      id: eventCounter,
      name,
      payload,
      emittedAt: new Date().toISOString()
    };

    eventLog.push(event);
    if (eventLog.length > MAX_LOG_SIZE) {
      eventLog.shift();
    }

    if (hooks.has(name)) {
      try {
        hooks.get(name)(event);
      } catch (err) {
        console.error('[DaffyBridge] Hook error for ' + name + ':', err);
      }
    }

    return event;
  }

  function closeEventStream() {
    if (bridgeStream.source && typeof bridgeStream.source.close === 'function') {
      bridgeStream.source.close();
    }
    bridgeStream.source = null;
    bridgeStream.connected = false;
    return getStreamState();
  }

  function getStreamState() {
    return {
      url: bridgeStream.url,
      connected: bridgeStream.connected,
      lastError: bridgeStream.lastError,
      reconnects: bridgeStream.reconnects
    };
  }

  function dispatchBridgeEnvelope(envelope) {
    if (!envelope || typeof envelope.name !== 'string' || !envelope.name) {
      emit('bridge:stream-error', { error: 'invalid-envelope', envelope: envelope });
      return;
    }
    emit(envelope.name, envelope.payload || {});
  }

  function connectEventStream(url) {
    if (!url) {
      bridgeStream.lastError = 'No bridge URL configured';
      emit('bridge:stream-error', { error: bridgeStream.lastError });
      return getStreamState();
    }
    if (typeof global.EventSource !== 'function') {
      bridgeStream.lastError = 'EventSource is unavailable in this environment';
      emit('bridge:stream-error', { error: bridgeStream.lastError, url: url });
      return getStreamState();
    }

    if (bridgeStream.source) {
      closeEventStream();
      bridgeStream.reconnects++;
    }

    bridgeStream.url = url;
    bridgeStream.lastError = '';
    bridgeStream.connected = false;
    streamCounter++;

    try {
      bridgeStream.source = new global.EventSource(url);
    } catch (err) {
      bridgeStream.lastError = 'EventSource creation failed: ' + err.message;
      emit('bridge:stream-error', { error: bridgeStream.lastError, url: url });
      return getStreamState();
    }

    bridgeStream.source.onopen = function () {
      bridgeStream.connected = true;
      bridgeStream.lastError = '';
      emit('bridge:stream-open', { url: url, connection: streamCounter });
    };

    bridgeStream.source.onmessage = function (event) {
      try {
        dispatchBridgeEnvelope(JSON.parse(event.data));
      } catch (err) {
        bridgeStream.lastError = 'Invalid SSE payload: ' + err.message;
        emit('bridge:stream-error', { error: bridgeStream.lastError, url: url, raw: event.data });
      }
    };

    bridgeStream.source.onerror = function () {
      bridgeStream.connected = false;
      bridgeStream.lastError = 'EventSource stream error';
      emit('bridge:stream-error', { error: bridgeStream.lastError, url: url, reconnects: bridgeStream.reconnects });
    };

    return getStreamState();
  }

  function getEventLog() {
    return eventLog.slice();
  }

  function getStats() {
      return {
      hooksRegistered: hooks.size,
      eventsEmitted: eventCounter,
      logSize: eventLog.length,
      hookNames: Array.from(hooks.keys()),
      stream: getStreamState()
    };
  }

  function roomShell() {
    return {
      lastEvent: 'No bridge events yet.',
      voiceTransport: 'native-client-only',
      bridgeStats: getStats(),
      init() {
        registerHook('sample:bridge-ready', (event) => {
          this.lastEvent = JSON.stringify(event, null, 2);
        });
        registerHook('bridge:stream-open', () => {
          this.bridgeStats = getStats();
        });
        registerHook('voice:state-changed', (event) => {
          if (event.payload && event.payload.transport) {
            this.voiceTransport = event.payload.transport;
          }
        });
      },
      emitSampleEvent() {
        const event = emit('sample:bridge-ready', {
          source: 'frontend-room-shell',
          message: 'Tier 0 bridge bootstrap is alive.'
        });
        this.lastEvent = JSON.stringify(event, null, 2);
        this.bridgeStats = getStats();
      }
    };
  }

  function createSignalingClient(config) {
    const transport = config.transport === 'socketio' ? 'socketio' : 'websocket';
    const state = {
      url: config.url || '',
      room: config.room || '',
      peerId: config.peerId || '',
      transport: transport,
      websocket: null,
      socketio: null,
      connected: false,
      joined: false,
      outbound: 0,
      inbound: 0,
      lastError: ''
    };

    function connect() {
      if (!state.url) {
        state.lastError = 'No signaling URL configured';
        emit('signaling:error', { error: state.lastError });
        return;
      }

      emit('signaling:connecting', { url: state.url });
      if (state.transport === 'socketio') {
        connectSocketIo();
      } else {
        connectWebSocket();
      }
    }

    function onSignalingPayload(message) {
      emit('signaling:message', message);
      if (message.type === 'join-ack') {
        state.joined = true;
        emit('signaling:joined', message);
      } else if (message.type === 'peer-ready') {
        emit('room:participant-joined', { peerId: message.peer_id });
      } else if (message.type === 'peer-left') {
        emit('room:participant-left', { peerId: message.peer_id });
      } else if (message.type === 'error') {
        state.lastError = message.error || 'Unknown signaling error';
        emit('signaling:error', { error: state.lastError });
      }
    }

    function connectWebSocket() {
      try {
        state.websocket = new WebSocket(state.url);
      } catch (err) {
        state.lastError = 'WebSocket creation failed: ' + err.message;
        emit('signaling:error', { error: state.lastError });
        return;
      }

      state.websocket.onopen = function () {
        state.connected = true;
        state.lastError = '';
        emit('signaling:connected', { url: state.url, transport: state.transport });
        if (state.room && state.peerId) {
          send({ type: 'join', room: state.room, peer_id: state.peerId });
        }
      };
      state.websocket.onmessage = function (event) {
        state.inbound++;
        try {
          onSignalingPayload(JSON.parse(event.data));
        } catch (err) {
          console.error('[DaffyBridge] Failed to parse signaling message:', err);
        }
      };
      state.websocket.onclose = function () {
        state.connected = false;
        state.joined = false;
        emit('signaling:disconnected', { url: state.url, transport: state.transport });
      };
      state.websocket.onerror = function () {
        state.lastError = 'WebSocket error';
        emit('signaling:error', { error: state.lastError });
      };
    }

    function connectSocketIo() {
      if (typeof global.io !== 'function') {
        state.lastError = 'Socket.IO client unavailable (load frontend/lib/socket.io.min.js)';
        emit('signaling:error', { error: state.lastError });
        return;
      }
      state.socketio = global.io(state.url, {
        path: config.socketIoPath || '/socket.io',
        transports: ['websocket'],
        query: { browser: '1' }
      });
      state.socketio.on('connect', function () {
        state.connected = true;
        state.lastError = '';
        emit('signaling:connected', { url: state.url, transport: state.transport });
        if (state.room && state.peerId) {
          send({ type: 'join', room: state.room, peer_id: state.peerId });
        }
      });
      state.socketio.on('signal', function (message) {
        state.inbound++;
        onSignalingPayload(message || {});
      });
      state.socketio.on('disconnect', function () {
        state.connected = false;
        state.joined = false;
        emit('signaling:disconnected', { url: state.url, transport: state.transport });
      });
      state.socketio.on('connect_error', function (err) {
        state.lastError = 'Socket.IO error: ' + (err && err.message ? err.message : 'unknown');
        emit('signaling:error', { error: state.lastError });
      });
    }

    function send(message) {
      if (state.transport === 'socketio' && state.socketio && state.connected) {
        state.socketio.emit('signal', message);
        state.outbound++;
        return true;
      }
      if (state.websocket && state.websocket.readyState === WebSocket.OPEN) {
        state.websocket.send(JSON.stringify(message));
        state.outbound++;
        return true;
      }
      return false;
    }

    function disconnect() {
      if (state.websocket) {
        state.websocket.close();
        state.websocket = null;
      }
      if (state.socketio) {
        state.socketio.disconnect();
        state.socketio = null;
      }
      state.connected = false;
      state.joined = false;
    }

    function snapshot() {
      return {
        websocket: state.connected ? 'open' : 'closed',
        transport: state.transport,
        room: state.room,
        peerId: state.peerId,
        outbound: state.outbound,
        inbound: state.inbound,
        joined: state.joined,
        lastError: state.lastError
      };
    }

    return { connect, send, disconnect, snapshot, state };
  }

  global.DaffyBridge = {
    emit,
    registerHook,
    removeHook,
    connectEventStream,
    disconnectEventStream: closeEventStream,
    getStreamState,
    roomShell,
    getBootstrapConfig: readBootstrapConfig,
    persistBootstrapConfig,
    getEventLog,
    getStats,
    createSignalingClient,
    _hooks: hooks
  };

  emit('bridge:bootstrap', readBootstrapConfig());
})(window);
