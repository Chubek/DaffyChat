Control Plane
=============

The control plane manages room lifecycle, participant tracking, service orchestration, and event coordination.

Components
----------

Backend Server
~~~~~~~~~~~~~~

The main backend server handles:

* Room registry and lifecycle management
* Participant session tracking
* Service metadata and discovery
* Frontend bridge event emission
* REST API endpoints

**Key responsibilities:**

* Create, list, update, delete rooms
* Track active participants and sessions
* Coordinate service startup and shutdown
* Emit events to frontend via bridge

Daemon Manager (daffydmd)
~~~~~~~~~~~~~~~~~~~~~~~~~~

The daemon manager orchestrates service lifecycle:

* Start, stop, restart services
* Monitor service health
* Manage service dependencies
* Persist service state
* Handle service crashes and restarts

**Implementation:** ``src/runtime/daemon_manager.cpp``

Service Registry
~~~~~~~~~~~~~~~~

Tracks available services and their metadata:

* Service name and version
* IPC socket path
* Service status (active, stopped, failed)
* Startup dependencies
* Health check configuration

**Storage:** JSON file at ``~/.config/daffychat/services.json``

Room Runtime
~~~~~~~~~~~~

Manages room instances and their isolation:

* Process-based isolation (Tier 0/1)
* LXC container isolation (future)
* Room state persistence
* Extension loading and execution
* Event bus integration

**Implementation:** ``src/rooms/room_runtime.cpp``

Architecture
------------

Service Communication
~~~~~~~~~~~~~~~~~~~~~

Services communicate via NNG IPC transport:

.. code-block:: text

   ┌─────────────┐
   │   Backend   │
   └──────┬──────┘
          │ IPC (NNG)
          │
   ┌──────┴──────────────────────┐
   │                             │
   ┌──────▼──────┐      ┌────────▼────────┐
   │ Echo Service│      │ Room Ops Service│
   └─────────────┘      └─────────────────┘
   /tmp/daffy-echo.ipc  /tmp/daffy-room_ops.ipc

JSON-RPC 2.0 Protocol
~~~~~~~~~~~~~~~~~~~~~~

All service communication uses JSON-RPC 2.0:

**Request:**

.. code-block:: json

   {
       "jsonrpc": "2.0",
       "method": "Echo",
       "params": {
           "message": "hello",
           "sender": "alice"
       },
       "id": 1
   }

**Response:**

.. code-block:: json

   {
       "jsonrpc": "2.0",
       "result": {
           "message": "hello",
           "sender": "alice",
           "echoed": true
       },
       "id": 1
   }

Room Lifecycle
--------------

Room States
~~~~~~~~~~~

Rooms transition through these states:

1. **Created** - Room registered but not started
2. **Starting** - Room runtime initializing
3. **Active** - Room accepting participants
4. **Paused** - Room temporarily suspended
5. **Stopping** - Room shutting down
6. **Stopped** - Room terminated

State Transitions
~~~~~~~~~~~~~~~~~

.. code-block:: text

   Created → Starting → Active → Paused → Active
                          ↓
                      Stopping → Stopped

Room Creation Flow
~~~~~~~~~~~~~~~~~~

1. Client sends create room request
2. Backend validates room configuration
3. Backend allocates room ID
4. Backend creates room runtime instance
5. Runtime loads extensions
6. Runtime initializes event bus
7. Room transitions to Active state
8. Backend returns room metadata to client

Participant Management
----------------------

Session Tracking
~~~~~~~~~~~~~~~~

Each participant has a session:

* Session ID (unique)
* User ID
* Room ID
* Join timestamp
* Last seen timestamp
* Connection state

Session Lifecycle
~~~~~~~~~~~~~~~~~

1. **Join** - User requests to join room
2. **Authenticate** - Verify user credentials
3. **Allocate** - Create session record
4. **Subscribe** - Subscribe to room events
5. **Active** - Participant in room
6. **Leave** - User leaves or disconnects
7. **Cleanup** - Remove session and unsubscribe

Event Coordination
------------------

Event Bus
~~~~~~~~~

The event bus coordinates events across services:

* Room events (join, leave, message)
* Service events (start, stop, health)
* Extension events (custom handlers)

**Implementation:** ``src/events/event_bus.cpp``

Event Flow
~~~~~~~~~~

.. code-block:: text

   Participant → Room Runtime → Event Bus → Extensions
                                    ↓
                              Frontend Bridge

Event Types
~~~~~~~~~~~

* ``room.join`` - Participant joined
* ``room.leave`` - Participant left
* ``room.message`` - Message posted
* ``room.state_change`` - Room state changed
* ``service.started`` - Service started
* ``service.stopped`` - Service stopped
* ``extension.loaded`` - Extension loaded

Service Discovery
-----------------

Services register with the daemon manager on startup:

1. Service starts and binds to IPC socket
2. Service sends registration request to daemon manager
3. Daemon manager validates service metadata
4. Daemon manager adds service to registry
5. Daemon manager broadcasts service availability

Health Monitoring
-----------------

The daemon manager monitors service health:

* Periodic health checks via ``Status()`` RPC
* Process monitoring (PID tracking)
* Socket availability checks
* Automatic restart on failure

Configuration
-------------

Control plane configuration in ``config/daffychat.json``:

.. code-block:: json

   {
       "server": {
           "host": "0.0.0.0",
           "port": 8080
       },
       "runtime_isolation": {
           "mode": "process",
           "lxc_enabled": false
       },
       "services": {
           "autostart": ["echo", "room_ops", "bot_api"],
           "health_check_interval": 30
       }
   }

See Also
--------

* :doc:`overview` - Architecture overview
* :doc:`media-plane` - Voice transport
* :doc:`extension-plane` - Extension system
