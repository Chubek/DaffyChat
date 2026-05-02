/**
 * Socket.IO Voice Transport Client
 * Provides WebRTC voice communication via Socket.IO signaling
 */

class SocketIOVoiceTransport {
  constructor(config = {}) {
    this.config = {
      serverUrl: config.serverUrl || 'http://localhost:7002',
      peerId: config.peerId || 'web-' + Math.random().toString(36).substr(2, 9),
      room: config.room || 'default',
      iceServers: config.iceServers || [
        { urls: 'stun:stun.l.google.com:19302' }
      ],
      debug: config.debug || false
    };

    this.socket = null;
    this.peerConnection = null;
    this.localStream = null;
    this.remoteStream = null;
    this.connectionId = null;
    this.connected = false;
    this.inCall = false;

    this.eventHandlers = {
      onConnected: null,
      onDisconnected: null,
      onRemoteStream: null,
      onError: null,
      onPeerReady: null,
      onCallStarted: null,
      onCallEnded: null
    };
  }

  /**
   * Initialize Socket.IO connection
   */
  async connect() {
    return new Promise((resolve, reject) => {
      try {
        this.socket = io(this.config.serverUrl, {
          transports: ['websocket', 'polling']
        });

        this.socket.on('connect', () => {
          this.log('Socket.IO connected');
          this.socket.emit('connect', { peer_id: this.config.peerId });
        });

        this.socket.on('connected', (data) => {
          this.connectionId = data.connection_id;
          this.connected = true;
          this.log('Session established:', data);
          
          if (this.eventHandlers.onConnected) {
            this.eventHandlers.onConnected(data);
          }

          resolve(data);
        });

        this.socket.on('signal', (message) => {
          this.handleSignalMessage(JSON.parse(message));
        });

        this.socket.on('disconnect', () => {
          this.log('Socket.IO disconnected');
          this.connected = false;
          this.cleanup();
          
          if (this.eventHandlers.onDisconnected) {
            this.eventHandlers.onDisconnected();
          }
        });

        this.socket.on('error', (error) => {
          this.log('Socket.IO error:', error);
          if (this.eventHandlers.onError) {
            this.eventHandlers.onError(error);
          }
          reject(error);
        });

      } catch (error) {
        this.log('Connection error:', error);
        reject(error);
      }
    });
  }

  /**
   * Join a voice room
   */
  async joinRoom(room) {
    if (!this.connected) {
      throw new Error('Not connected to signaling server');
    }

    this.config.room = room || this.config.room;

    this.sendSignal({
      type: 'join',
      room: this.config.room,
      peer_id: this.config.peerId
    });

    this.log('Joining room:', this.config.room);
  }

  /**
   * Start voice call (get local media and create peer connection)
   */
  async startCall() {
    try {
      // Get local audio stream
      this.localStream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,
          noiseSuppression: true,
          autoGainControl: true
        },
        video: false
      });

      this.log('Got local stream');

      // Create peer connection
      this.peerConnection = new RTCPeerConnection({
        iceServers: this.config.iceServers
      });

      // Add local tracks
      this.localStream.getTracks().forEach(track => {
        this.peerConnection.addTrack(track, this.localStream);
      });

      // Handle ICE candidates
      this.peerConnection.onicecandidate = (event) => {
        if (event.candidate) {
          this.sendSignal({
            type: 'ice-candidate',
            room: this.config.room,
            peer_id: this.config.peerId,
            candidate: event.candidate.candidate,
            sdpMid: event.candidate.sdpMid,
            sdpMLineIndex: event.candidate.sdpMLineIndex
          });
        }
      };

      // Handle remote stream
      this.peerConnection.ontrack = (event) => {
        this.log('Received remote track');
        this.remoteStream = event.streams[0];
        
        if (this.eventHandlers.onRemoteStream) {
          this.eventHandlers.onRemoteStream(this.remoteStream);
        }
      };

      // Handle connection state changes
      this.peerConnection.onconnectionstatechange = () => {
        this.log('Connection state:', this.peerConnection.connectionState);
        
        if (this.peerConnection.connectionState === 'connected') {
          this.inCall = true;
          if (this.eventHandlers.onCallStarted) {
            this.eventHandlers.onCallStarted();
          }
        } else if (this.peerConnection.connectionState === 'disconnected' ||
                   this.peerConnection.connectionState === 'failed') {
          this.inCall = false;
          if (this.eventHandlers.onCallEnded) {
            this.eventHandlers.onCallEnded();
          }
        }
      };

      this.log('Peer connection created');
      return this.localStream;

    } catch (error) {
      this.log('Error starting call:', error);
      if (this.eventHandlers.onError) {
        this.eventHandlers.onError(error);
      }
      throw error;
    }
  }

  /**
   * Create and send WebRTC offer
   */
  async createOffer() {
    if (!this.peerConnection) {
      throw new Error('Peer connection not initialized');
    }

    try {
      const offer = await this.peerConnection.createOffer();
      await this.peerConnection.setLocalDescription(offer);

      this.sendSignal({
        type: 'offer',
        room: this.config.room,
        peer_id: this.config.peerId,
        sdp: offer.sdp
      });

      this.log('Offer created and sent');
    } catch (error) {
      this.log('Error creating offer:', error);
      throw error;
    }
  }

  /**
   * Handle incoming signaling messages
   */
  async handleSignalMessage(message) {
    this.log('Received signal:', message.type);

    try {
      switch (message.type) {
        case 'join-ack':
          this.log('Joined room:', message.room);
          break;

        case 'peer-ready':
          this.log('Peer ready:', message.peer_id);
          if (this.eventHandlers.onPeerReady) {
            this.eventHandlers.onPeerReady(message);
          }
          // If we have a peer connection, create an offer
          if (this.peerConnection) {
            await this.createOffer();
          }
          break;

        case 'offer':
          await this.handleOffer(message);
          break;

        case 'answer':
          await this.handleAnswer(message);
          break;

        case 'ice-candidate':
          await this.handleIceCandidate(message);
          break;

        case 'peer-left':
          this.log('Peer left:', message.peer_id);
          this.cleanup();
          break;

        case 'error':
          this.log('Server error:', message.error);
          if (this.eventHandlers.onError) {
            this.eventHandlers.onError(message.error);
          }
          break;

        default:
          this.log('Unknown message type:', message.type);
      }
    } catch (error) {
      this.log('Error handling signal:', error);
      if (this.eventHandlers.onError) {
        this.eventHandlers.onError(error);
      }
    }
  }

  /**
   * Handle incoming WebRTC offer
   */
  async handleOffer(message) {
    if (!this.peerConnection) {
      await this.startCall();
    }

    const offer = new RTCSessionDescription({
      type: 'offer',
      sdp: message.sdp
    });

    await this.peerConnection.setRemoteDescription(offer);
    const answer = await this.peerConnection.createAnswer();
    await this.peerConnection.setLocalDescription(answer);

    this.sendSignal({
      type: 'answer',
      room: this.config.room,
      peer_id: this.config.peerId,
      target_peer_id: message.peer_id,
      sdp: answer.sdp
    });

    this.log('Answer created and sent');
  }

  /**
   * Handle incoming WebRTC answer
   */
  async handleAnswer(message) {
    const answer = new RTCSessionDescription({
      type: 'answer',
      sdp: message.sdp
    });

    await this.peerConnection.setRemoteDescription(answer);
    this.log('Answer received and set');
  }

  /**
   * Handle incoming ICE candidate
   */
  async handleIceCandidate(message) {
    if (!this.peerConnection) {
      this.log('Received ICE candidate but no peer connection');
      return;
    }

    const candidate = new RTCIceCandidate({
      candidate: message.candidate,
      sdpMid: message.sdpMid,
      sdpMLineIndex: message.sdpMLineIndex
    });

    await this.peerConnection.addIceCandidate(candidate);
    this.log('ICE candidate added');
  }

  /**
   * Send signaling message
   */
  sendSignal(message) {
    if (!this.socket || !this.connected) {
      this.log('Cannot send signal: not connected');
      return;
    }

    this.socket.emit('signal', JSON.stringify(message));
    this.log('Sent signal:', message.type);
  }

  /**
   * Leave the current room
   */
  leaveRoom() {
    if (this.connected && this.config.room) {
      this.sendSignal({
        type: 'leave',
        room: this.config.room,
        peer_id: this.config.peerId
      });
    }
    this.cleanup();
  }

  /**
   * Disconnect from signaling server
   */
  disconnect() {
    this.leaveRoom();
    
    if (this.socket) {
      this.socket.disconnect();
      this.socket = null;
    }
    
    this.connected = false;
  }

  /**
   * Clean up resources
   */
  cleanup() {
    if (this.peerConnection) {
      this.peerConnection.close();
      this.peerConnection = null;
    }

    if (this.localStream) {
      this.localStream.getTracks().forEach(track => track.stop());
      this.localStream = null;
    }

    this.remoteStream = null;
    this.inCall = false;
  }

  /**
   * Mute/unmute local audio
   */
  setMuted(muted) {
    if (this.localStream) {
      this.localStream.getAudioTracks().forEach(track => {
        track.enabled = !muted;
      });
    }
  }

  /**
   * Check if local audio is muted
   */
  isMuted() {
    if (!this.localStream) return true;
    const audioTrack = this.localStream.getAudioTracks()[0];
    return audioTrack ? !audioTrack.enabled : true;
  }

  /**
   * Register event handler
   */
  on(event, handler) {
    if (this.eventHandlers.hasOwnProperty('on' + event.charAt(0).toUpperCase() + event.slice(1))) {
      this.eventHandlers['on' + event.charAt(0).toUpperCase() + event.slice(1)] = handler;
    }
  }

  /**
   * Debug logging
   */
  log(...args) {
    if (this.config.debug) {
      console.log('[SocketIOVoiceTransport]', ...args);
    }
  }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
  module.exports = SocketIOVoiceTransport;
}
