Extension Plane
===============

The extension plane provides customizable room logic through DSSL microservices and Daffyscript WASM extensions.

Overview
--------

DaffyChat's extension system has two layers:

1. **DSSL Services** - Type-safe microservices for core functionality
2. **Daffyscript Extensions** - WASM-based room-specific logic

Both run server-side, not in browsers, ensuring security and consistency.

DSSL Services
-------------

Service Architecture
~~~~~~~~~~~~~~~~~~~~

DSSL services are independent daemons that:

* Define APIs via DSSL specifications
* Generate type-safe C++ implementations
* Communicate via NNG IPC transport
* Handle JSON-RPC 2.0 requests
* Register with daemon manager

**Example services:**

* ``echo`` - Simple echo service
* ``room_ops`` - Room join/leave operations
* ``bot_api`` - Bot integration API
* ``event_bridge`` - Event routing

Service Lifecycle
~~~~~~~~~~~~~~~~~

1. **Define** - Write DSSL specification
2. **Generate** - Run dssl-bindgen to create C++ code
3. **Implement** - Fill in service logic
4. **Build** - Compile service binary
5. **Register** - Install with daemon manager
6. **Deploy** - Start service via daffydmd

**See:** :doc:`../dssl/getting-started` for detailed tutorial

Service Communication
~~~~~~~~~~~~~~~~~~~~~

Services communicate via IPC sockets:

.. code-block:: text

   ┌──────────────┐
   │ Room Runtime │
   └──────┬───────┘
          │ IPC
   ┌──────┴──────────────────┐
   │                         │
   ┌──────▼──────┐    ┌──────▼──────┐
   │  Bot API    │    │ Event Bridge│
   └─────────────┘    └─────────────┘

Daffyscript Extensions
----------------------

Extension Types
~~~~~~~~~~~~~~~

**Modules**
  Reusable libraries with exported functions:

  .. code-block:: javascript

     export function greet(name) {
         return "Hello, " + name;
     }

**Programs**
  Standalone scripts that execute on events:

  .. code-block:: javascript

     function onMessage(event) {
         if (event.text.startsWith("!help")) {
             return {type: "message", text: "Available commands: ..."};
         }
     }

**Recipes**
  Room configuration and initialization:

  .. code-block:: json

     {
         "name": "Gaming Room",
         "max_participants": 20,
         "extensions": ["game-logic", "leaderboard"]
     }

WASM Runtime
~~~~~~~~~~~~

Extensions run in a WebAssembly sandbox:

* **Isolation** - Cannot access host system
* **Performance** - Near-native execution speed
* **Portability** - Same bytecode runs everywhere
* **Safety** - Memory-safe execution

**Implementation:** ``frontend/lib/wasm-runtime.js`` (10KB)

Extension Manager
~~~~~~~~~~~~~~~~~

Manages extension lifecycle:

* Load WASM modules
* Initialize extension state
* Route events to handlers
* Manage extension storage
* Handle extension errors

**Implementation:** ``frontend/app/api/extension-manager.js`` (403 lines)

Event System
------------

Event Flow
~~~~~~~~~~

.. code-block:: text

   Room Event → Event Bus → Extension Manager → WASM Handler
                                    ↓
                            Extension Response
                                    ↓
                              Room Runtime

Event Types
~~~~~~~~~~~

Extensions can handle these events:

* ``onMessage`` - Message posted to room
* ``onJoin`` - Participant joined
* ``onLeave`` - Participant left
* ``onCommand`` - Bot command issued
* ``onTimer`` - Scheduled timer fired
* ``onStateChange`` - Room state changed

Event Handlers
~~~~~~~~~~~~~~

Handlers receive event objects and return responses:

.. code-block:: javascript

   export function onMessage(event) {
       // event = {
       //   type: "message",
       //   text: "hello",
       //   sender: "alice",
       //   timestamp: "2025-01-01T00:00:00Z"
       // }
       
       if (event.text.startsWith("!ping")) {
           return {
               type: "message",
               text: "Pong!"
           };
       }
   }

Extension APIs
--------------

Room API
~~~~~~~~

Access room state and participants:

.. code-block:: javascript

   // Get room info
   const room = Room.getInfo();
   
   // List participants
   const participants = Room.getParticipants();
   
   // Get participant by ID
   const user = Room.getParticipant("alice");

Bot API
~~~~~~~

Send messages and perform actions:

.. code-block:: javascript

   // Send message
   Bot.sendMessage("Hello, room!");
   
   // Kick participant
   Bot.kick("alice", "Spam");
   
   // Mute participant
   Bot.mute("bob", 300); // 5 minutes

Storage API
~~~~~~~~~~~

Persistent key-value storage:

.. code-block:: javascript

   // Store value
   Storage.set("counter", 42);
   
   // Retrieve value
   const count = Storage.get("counter");
   
   // Delete value
   Storage.delete("counter");
   
   // List keys
   const keys = Storage.keys();

Timer API
~~~~~~~~~

Schedule delayed or periodic execution:

.. code-block:: javascript

   // One-time timer
   Timer.once(5000, () => {
       Bot.sendMessage("5 seconds elapsed");
   });
   
   // Periodic timer
   Timer.repeat(60000, () => {
       Bot.sendMessage("1 minute tick");
   });

Extension Development
---------------------

Creating an Extension
~~~~~~~~~~~~~~~~~~~~~

1. Write Daffyscript code:

   .. code-block:: javascript

      // greeter.dfy
      export function onJoin(event) {
          return {
              type: "message",
              text: "Welcome, " + event.user + "!"
          };
      }

2. Compile to WASM:

   .. code-block:: bash

      daffyscript compile greeter.dfy -o greeter.wasm

3. Load in room:

   .. code-block:: bash

      daffychat load-extension my-room greeter.wasm

Testing Extensions
~~~~~~~~~~~~~~~~~~

Test locally before deployment:

.. code-block:: bash

   # Run in test mode
   daffyscript test greeter.dfy

   # Simulate events
   daffyscript test greeter.dfy --event join --user alice

Debugging Extensions
~~~~~~~~~~~~~~~~~~~~

Extensions can log to console:

.. code-block:: javascript

   export function onMessage(event) {
       console.log("Received message:", event.text);
       // ...
   }

Logs appear in room runtime logs.

Security
--------

Sandboxing
~~~~~~~~~~

Extensions run in isolated WASM sandbox:

* No file system access
* No network access
* No system calls
* Limited memory (configurable)
* CPU time limits

Resource Limits
~~~~~~~~~~~~~~~

Extensions are subject to limits:

* **Memory:** 10MB per extension
* **CPU time:** 100ms per event handler
* **Storage:** 1MB per extension
* **Timers:** 10 active timers per extension

Violations result in extension termination.

Permissions
~~~~~~~~~~~

Extensions declare required permissions:

.. code-block:: json

   {
       "name": "moderator-bot",
       "permissions": [
           "bot.send_message",
           "bot.kick",
           "bot.mute"
       ]
   }

Users must approve permissions before loading.

Extension Examples
------------------

Greeting Bot
~~~~~~~~~~~~

.. code-block:: javascript

   export function onJoin(event) {
       return {
           type: "message",
           text: "Welcome to the room, " + event.user + "!"
       };
   }

Command Handler
~~~~~~~~~~~~~~~

.. code-block:: javascript

   export function onMessage(event) {
       if (!event.text.startsWith("!")) return;
       
       const [cmd, ...args] = event.text.slice(1).split(" ");
       
       switch (cmd) {
           case "help":
               return {type: "message", text: "Commands: !help, !ping, !time"};
           case "ping":
               return {type: "message", text: "Pong!"};
           case "time":
               return {type: "message", text: new Date().toISOString()};
       }
   }

Moderation Bot
~~~~~~~~~~~~~~

.. code-block:: javascript

   const warnings = {};
   
   export function onMessage(event) {
       if (containsBadWords(event.text)) {
           warnings[event.sender] = (warnings[event.sender] || 0) + 1;
           
           if (warnings[event.sender] >= 3) {
               Bot.kick(event.sender, "Repeated violations");
           } else {
               return {
                   type: "message",
                   text: "Warning: Please keep chat clean"
               };
           }
       }
   }

Configuration
-------------

Extension configuration in ``config/daffychat.json``:

.. code-block:: json

   {
       "extensions": {
           "wasm_memory_limit": 10485760,
           "cpu_time_limit_ms": 100,
           "storage_limit": 1048576,
           "max_timers": 10
       }
   }

See Also
--------

* :doc:`overview` - Architecture overview
* :doc:`../dssl/index` - DSSL documentation
* :doc:`../daffyscript/index` - Daffyscript documentation
