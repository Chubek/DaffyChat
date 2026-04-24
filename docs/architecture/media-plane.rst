Media Plane
===========

The media plane handles real-time voice transport using peer-to-peer WebRTC with high-quality audio processing.

Overview
--------

DaffyChat's voice architecture follows a peer-to-peer model:

* Native clients establish direct WebRTC connections
* Signaling server coordinates connection setup
* Audio processing pipeline ensures high quality
* SRTP encryption secures media streams

**Note:** Browser-based voice is explicitly out of scope for the MVP.

Components
----------

Signaling Server
~~~~~~~~~~~~~~~~

Coordinates WebRTC connection establishment:

* Handles SDP offer/answer exchange
* Relays ICE candidates between peers
* Manages room-based signaling channels
* Built with uWebSockets for performance

**Implementation:** ``src/signaling/signaling_server.cpp``

**Protocol:** WebSocket-based signaling

WebRTC Stack
~~~~~~~~~~~~

Peer connection management via libdatachannel:

* ICE (Interactive Connectivity Establishment)
* DTLS (Datagram Transport Layer Security)
* SRTP (Secure Real-time Transport Protocol)
* RTP (Real-time Transport Protocol)

**Library:** libdatachannel

Audio Pipeline
~~~~~~~~~~~~~~

High-quality audio processing chain:

1. **Capture** - PortAudio input stream
2. **Noise Suppression** - RNNoise processing
3. **Resampling** - libsamplerate (if needed)
4. **Encoding** - Opus codec
5. **Transmission** - RTP over SRTP
6. **Reception** - RTP over SRTP
7. **Decoding** - Opus codec
8. **Playback** - PortAudio output stream

**Implementation:** ``src/voice/audio_pipeline.cpp``

Architecture
------------

Connection Flow
~~~~~~~~~~~~~~~

.. code-block:: text

   Client A                Signaling Server              Client B
      |                           |                          |
      |--- join room ------------>|                          |
      |                           |<--- join room -----------|
      |                           |                          |
      |--- create offer --------->|                          |
      |                           |--- offer --------------->|
      |                           |                          |
      |                           |<--- answer --------------|
      |<--- answer ---------------|                          |
      |                           |                          |
      |--- ICE candidate -------->|--- ICE candidate ------->|
      |<--- ICE candidate --------|<--- ICE candidate -------|
      |                           |                          |
      |<========== Direct P2P Connection ==================>|

Signaling Protocol
~~~~~~~~~~~~~~~~~~

WebSocket messages for signaling:

**Join Room:**

.. code-block:: json

   {
       "type": "join",
       "room_id": "room-123",
       "user_id": "alice"
   }

**Offer:**

.. code-block:: json

   {
       "type": "offer",
       "room_id": "room-123",
       "from": "alice",
       "to": "bob",
       "sdp": "v=0\r\no=- ..."
   }

**Answer:**

.. code-block:: json

   {
       "type": "answer",
       "room_id": "room-123",
       "from": "bob",
       "to": "alice",
       "sdp": "v=0\r\no=- ..."
   }

**ICE Candidate:**

.. code-block:: json

   {
       "type": "ice-candidate",
       "room_id": "room-123",
       "from": "alice",
       "to": "bob",
       "candidate": "candidate:1 1 UDP ..."
   }

Audio Processing
----------------

Capture Pipeline
~~~~~~~~~~~~~~~~

PortAudio captures audio from the microphone:

* Sample rate: 48 kHz
* Channels: Mono
* Format: Float32
* Frame size: 480 samples (10ms at 48kHz)

**Implementation:** ``src/voice/portaudio_streams.cpp``

Noise Suppression
~~~~~~~~~~~~~~~~~

RNNoise removes background noise:

* Input: 480 samples, mono, float32, 48kHz
* Processing: Recurrent neural network
* Output: Cleaned audio with same format

**Requirements:**

* Must receive exactly 480 samples per frame
* Must be called at 48kHz sample rate
* Must be allocation-free (real-time constraint)

**Implementation:** ``src/voice/audio_processing.cpp``

Opus Encoding
~~~~~~~~~~~~~

Opus codec compresses audio for transmission:

* Bitrate: 24-64 kbps (configurable)
* Complexity: 10 (highest quality)
* Frame size: 10ms (480 samples at 48kHz)
* VBR (Variable Bit Rate) enabled

**Implementation:** ``src/voice/opus_codec.cpp``

SRTP Encryption
~~~~~~~~~~~~~~~

Secure RTP protects media streams:

* Encryption: AES-128-CM
* Authentication: HMAC-SHA1-80
* Key derivation: DTLS-SRTP
* Master key: 128 bits
* Master salt: 112 bits

**Key Exchange:**

SRTP keys are derived from the DTLS handshake, never transmitted separately.

**Implementation:** ``src/voice/rtp_srtp.cpp``

Playback Pipeline
~~~~~~~~~~~~~~~~~

Received audio is decoded and played:

1. Receive encrypted RTP packet
2. Decrypt with SRTP
3. Extract Opus payload
4. Decode to PCM
5. Play via PortAudio

Real-time Constraints
---------------------

Audio Callback Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PortAudio callbacks must be:

* **Non-blocking** - No I/O, no locks, no allocations
* **Fast** - Complete within frame time (10ms)
* **Deterministic** - Consistent execution time

**Violations cause:**

* Audio glitches
* Buffer underruns/overruns
* Dropped frames

Lock-free Design
~~~~~~~~~~~~~~~~

Audio pipeline uses lock-free data structures:

* Ring buffers for audio data
* Atomic operations for state
* Wait-free algorithms where possible

**Implementation:** ``include/daffy/voice/audio_pipeline.hpp``

Performance Targets
-------------------

Latency
~~~~~~~

* **Capture to encode:** < 10ms
* **Network transmission:** 20-100ms (depends on network)
* **Decode to playback:** < 10ms
* **Total end-to-end:** < 150ms (typical)

CPU Usage
~~~~~~~~~

* **RNNoise:** ~5% per stream (single core)
* **Opus encode/decode:** ~2% per stream
* **WebRTC stack:** ~3% per peer connection
* **Total per participant:** ~10% (single core)

Memory
~~~~~~

* **Audio buffers:** ~100KB per stream
* **WebRTC state:** ~50KB per peer connection
* **Total per participant:** ~200KB

Configuration
-------------

Voice configuration in ``config/daffychat.json``:

.. code-block:: json

   {
       "voice": {
           "sample_rate": 48000,
           "channels": 1,
           "frame_size": 480,
           "opus_bitrate": 32000,
           "opus_complexity": 10,
           "rnnoise_enabled": true
       },
       "turn": {
           "enabled": true,
           "server": "turn:turn.example.com:3478",
           "username": "user",
           "credential": "pass"
       }
   }

TURN Server
-----------

For NAT traversal when direct P2P fails:

* TURN server relays media when needed
* Configured via ``turn`` section in config
* Fallback when ICE fails to establish direct connection

**Note:** TURN adds latency and server load, used only when necessary.

Testing
-------

Loopback Test
~~~~~~~~~~~~~

Test audio pipeline without network:

.. code-block:: bash

   ./build/voice-loopback-test

This captures audio, processes it, and plays it back locally.

Echo Test
~~~~~~~~~

Test with echo service:

.. code-block:: bash

   ./build/voice-echo-test --room test-room

Network Test
~~~~~~~~~~~~

Test P2P connection between two clients:

.. code-block:: bash

   # Client 1
   ./build/voice-client --room test-room --user alice

   # Client 2
   ./build/voice-client --room test-room --user bob

See Also
--------

* :doc:`overview` - Architecture overview
* :doc:`control-plane` - Service orchestration
* :doc:`extension-plane` - Extension system
