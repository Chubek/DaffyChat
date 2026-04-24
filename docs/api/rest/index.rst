REST API Reference
==================

DaffyChat provides a REST API for room management and service interaction.

Base URL
--------

.. code-block:: text

   http://localhost:8080/api/v1

Authentication
--------------

API requests require authentication via Bearer token:

.. code-block:: http

   Authorization: Bearer <token>

Obtain a token via the ``/auth/login`` endpoint.

Endpoints
---------

Authentication
~~~~~~~~~~~~~~

Login
^^^^^

.. code-block:: http

   POST /api/v1/auth/login
   Content-Type: application/json

   {
       "username": "alice",
       "password": "secret"
   }

**Response:**

.. code-block:: json

   {
       "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
       "expires_at": "2025-01-02T00:00:00Z"
   }

Rooms
~~~~~

List Rooms
^^^^^^^^^^

.. code-block:: http

   GET /api/v1/rooms

**Response:**

.. code-block:: json

   {
       "rooms": [
           {
               "id": "room-123",
               "name": "General Chat",
               "participants": 5,
               "max_participants": 10,
               "created_at": "2025-01-01T00:00:00Z"
           }
       ]
   }

Get Room
^^^^^^^^

.. code-block:: http

   GET /api/v1/rooms/{room_id}

**Response:**

.. code-block:: json

   {
       "id": "room-123",
       "name": "General Chat",
       "description": "Main chat room",
       "participants": 5,
       "max_participants": 10,
       "state": "active",
       "created_at": "2025-01-01T00:00:00Z"
   }

Create Room
^^^^^^^^^^^

.. code-block:: http

   POST /api/v1/rooms
   Content-Type: application/json

   {
       "name": "My Room",
       "description": "A test room",
       "max_participants": 10
   }

**Response:**

.. code-block:: json

   {
       "id": "room-456",
       "name": "My Room",
       "description": "A test room",
       "max_participants": 10,
       "state": "active",
       "created_at": "2025-01-01T12:00:00Z"
   }

Update Room
^^^^^^^^^^^

.. code-block:: http

   PATCH /api/v1/rooms/{room_id}
   Content-Type: application/json

   {
       "name": "Updated Name",
       "max_participants": 20
   }

Delete Room
^^^^^^^^^^^

.. code-block:: http

   DELETE /api/v1/rooms/{room_id}

**Response:**

.. code-block:: json

   {
       "success": true
   }

Participants
~~~~~~~~~~~~

List Participants
^^^^^^^^^^^^^^^^^

.. code-block:: http

   GET /api/v1/rooms/{room_id}/participants

**Response:**

.. code-block:: json

   {
       "participants": [
           {
               "id": "alice",
               "display_name": "Alice",
               "joined_at": "2025-01-01T12:00:00Z",
               "state": "active"
           }
       ]
   }

Join Room
^^^^^^^^^

.. code-block:: http

   POST /api/v1/rooms/{room_id}/join

**Response:**

.. code-block:: json

   {
       "session_id": "session-789",
       "joined_at": "2025-01-01T12:00:00Z"
   }

Leave Room
^^^^^^^^^^

.. code-block:: http

   POST /api/v1/rooms/{room_id}/leave

**Response:**

.. code-block:: json

   {
       "success": true
   }

Services
~~~~~~~~

List Services
^^^^^^^^^^^^^

.. code-block:: http

   GET /api/v1/services

**Response:**

.. code-block:: json

   {
       "services": [
           {
               "name": "echo",
               "version": "1.0.0",
               "status": "active",
               "socket": "/tmp/daffy-echo.ipc"
           }
       ]
   }

Get Service Status
^^^^^^^^^^^^^^^^^^

.. code-block:: http

   GET /api/v1/services/{service_name}/status

**Response:**

.. code-block:: json

   {
       "name": "echo",
       "version": "1.0.0",
       "status": "active",
       "uptime_seconds": 3600,
       "pid": 12345
   }

Start Service
^^^^^^^^^^^^^

.. code-block:: http

   POST /api/v1/services/{service_name}/start

Stop Service
^^^^^^^^^^^^

.. code-block:: http

   POST /api/v1/services/{service_name}/stop

Restart Service
^^^^^^^^^^^^^^^

.. code-block:: http

   POST /api/v1/services/{service_name}/restart

Extensions
~~~~~~~~~~

List Extensions
^^^^^^^^^^^^^^^

.. code-block:: http

   GET /api/v1/rooms/{room_id}/extensions

**Response:**

.. code-block:: json

   {
       "extensions": [
           {
               "name": "greeter",
               "version": "1.0.0",
               "loaded_at": "2025-01-01T12:00:00Z"
           }
       ]
   }

Load Extension
^^^^^^^^^^^^^^

.. code-block:: http

   POST /api/v1/rooms/{room_id}/extensions
   Content-Type: multipart/form-data

   file: greeter.wasm

**Response:**

.. code-block:: json

   {
       "name": "greeter",
       "loaded_at": "2025-01-01T12:00:00Z"
   }

Unload Extension
^^^^^^^^^^^^^^^^

.. code-block:: http

   DELETE /api/v1/rooms/{room_id}/extensions/{extension_name}

Error Responses
---------------

All errors follow this format:

.. code-block:: json

   {
       "error": {
           "code": "ROOM_NOT_FOUND",
           "message": "Room with ID 'room-123' not found"
       }
   }

Common error codes:

* ``UNAUTHORIZED`` - Invalid or missing authentication
* ``FORBIDDEN`` - Insufficient permissions
* ``NOT_FOUND`` - Resource not found
* ``BAD_REQUEST`` - Invalid request parameters
* ``INTERNAL_ERROR`` - Server error

Rate Limiting
-------------

API requests are rate-limited:

* **Authenticated:** 1000 requests/hour
* **Unauthenticated:** 100 requests/hour

Rate limit headers:

.. code-block:: http

   X-RateLimit-Limit: 1000
   X-RateLimit-Remaining: 999
   X-RateLimit-Reset: 1640995200

See Also
--------

* :doc:`../cpp/index` - C++ API reference
* :doc:`../python/index` - Python API reference
* :doc:`../../operations/configuration` - API configuration
