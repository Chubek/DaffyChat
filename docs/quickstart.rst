Quickstart
==========

This guide gets you up and running with DaffyChat in 10 minutes.

Starting the System
-------------------

1. Start the daemon manager:

   .. code-block:: bash

      ./build/daffydmd start

2. Verify services are running:

   .. code-block:: bash

      ./build/daffydmd status

   You should see services like ``echo``, ``room_ops``, ``bot_api`` in the active state.

3. Start the frontend:

   .. code-block:: bash

      cd frontend
      npm run dev

4. Open your browser to http://localhost:3000

Creating Your First Room
------------------------

Using the CLI
~~~~~~~~~~~~~

.. code-block:: bash

   # Create a room recipe
   ./toolchain/dfc-mkrecipe.py \
       --name "My First Room" \
       --description "A test room" \
       --max-participants 10 \
       --output my-room.json

   # Create the room (via API or frontend)

Using the Frontend
~~~~~~~~~~~~~~~~~~

1. Navigate to http://localhost:3000
2. Click "Create Room"
3. Enter room name and settings
4. Click "Create"

Testing Services
----------------

Echo Service
~~~~~~~~~~~~

Test the echo service with a simple client:

.. code-block:: python

   import json
   import socket

   # Connect to echo service
   sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
   sock.connect('/tmp/daffy-echo.ipc')

   # Send JSON-RPC request
   request = {
       "jsonrpc": "2.0",
       "method": "Echo",
       "params": {
           "message": "Hello, DaffyChat!",
           "sender": "alice"
       },
       "id": 1
   }

   sock.send(json.dumps(request).encode())
   response = sock.recv(4096)
   print(json.loads(response))

Room Operations
~~~~~~~~~~~~~~~

.. code-block:: python

   # Join a room
   request = {
       "jsonrpc": "2.0",
       "method": "Join",
       "params": {"user": "alice"},
       "id": 1
   }

   # Leave a room
   request = {
       "jsonrpc": "2.0",
       "method": "Leave",
       "params": {"user": "alice"},
       "id": 2
   }

Creating a Simple Service
-------------------------

1. Scaffold a new service:

   .. code-block:: bash

      ./toolchain/dssl-init.py \
          --name greeter \
          --version 1.0.0 \
          --rpc Greet

2. Edit the DSSL spec (``greeter/specs/greeter.dssl``):

   .. code-block:: dssl

      service greeter 1.0.0;

      struct GreetReply {
          greeting: string;
          language: string;
      }

      rpc Greet(name: string, language: string) returns GreetReply;

3. Generate C++ code:

   .. code-block:: bash

      ./toolchain/dssl-bindgen.py \
          --target cpp \
          --out-dir ./greeter/generated \
          greeter/specs/greeter.dssl

4. Implement the service logic in ``greeter/src/greeter_service.cpp``

5. Build and install:

   .. code-block:: bash

      cd build
      cmake --build . -j2

      ./toolchain/install-service.py \
          --name greeter \
          --binary ./build/greeter-service \
          --socket /tmp/daffy-greeter.ipc

6. Start the service:

   .. code-block:: bash

      ./build/daffydmd start greeter

Writing a Daffyscript Extension
--------------------------------

1. Create a simple module (``my-extension.dfy``):

   .. code-block:: javascript

      // Daffyscript module
      export function onMessage(event) {
          if (event.text.startsWith("!hello")) {
              return {
                  type: "message",
                  text: "Hello, " + event.sender + "!"
              };
          }
      }

2. Compile to WASM:

   .. code-block:: bash

      daffyscript compile my-extension.dfy -o my-extension.wasm

3. Load in a room via the frontend extension manager

Common Tasks
------------

Managing Services
~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # List all services
   ./build/daffydmd list

   # Start a service
   ./build/daffydmd start <service-name>

   # Stop a service
   ./build/daffydmd stop <service-name>

   # Restart a service
   ./build/daffydmd restart <service-name>

   # Check service status
   ./build/daffydmd status <service-name>

Viewing Logs
~~~~~~~~~~~~

.. code-block:: bash

   # Service logs
   tail -f /var/log/daffy/<service-name>.log

   # Daemon manager logs
   tail -f /var/log/daffy/daffydmd.log

Configuration
~~~~~~~~~~~~~

Edit ``~/.config/daffychat/config.json`` to customize:

* Server ports
* TURN server settings
* Service configurations
* Frontend bridge settings

See :doc:`operations/configuration` for full details.

Next Steps
----------

* Read :doc:`dssl/getting-started` to create more complex services
* Explore :doc:`daffyscript/index` for extension development
* Review :doc:`architecture/overview` to understand the system design
* Check :doc:`api/cpp/index` for C++ API reference
