Configuration
=============

Complete configuration reference for DaffyChat.

Configuration File
------------------

Location
~~~~~~~~

DaffyChat looks for configuration in these locations (in order):

1. ``/etc/daffychat/config.json`` (system-wide)
2. ``~/.config/daffychat/config.json`` (user-specific)
3. ``./config.json`` (current directory)
4. Path specified via ``--config`` flag

Format
~~~~~~

Configuration uses JSON format:

.. code-block:: json

   {
       "server": { ... },
       "signaling": { ... },
       "turn": { ... },
       "runtime_isolation": { ... },
       "services": { ... },
       "frontend_bridge": { ... },
       "voice": { ... },
       "extensions": { ... },
       "database": { ... },
       "logging": { ... }
   }

Server Configuration
--------------------

.. code-block:: json

   {
       "server": {
           "host": "0.0.0.0",
           "port": 8080,
           "workers": 4,
           "max_connections": 1000,
           "request_timeout": 30,
           "cors_enabled": true,
           "cors_origins": ["*"]
       }
   }

Options
~~~~~~~

* ``host`` - Bind address (default: ``0.0.0.0``)
* ``port`` - HTTP port (default: ``8080``)
* ``workers`` - Worker threads (default: CPU cores)
* ``max_connections`` - Max concurrent connections (default: ``1000``)
* ``request_timeout`` - Request timeout in seconds (default: ``30``)
* ``cors_enabled`` - Enable CORS (default: ``true``)
* ``cors_origins`` - Allowed origins (default: ``["*"]``)

Signaling Configuration
-----------------------

.. code-block:: json

   {
       "signaling": {
           "host": "0.0.0.0",
           "port": 8081,
           "max_rooms": 100,
           "max_participants_per_room": 50,
           "ping_interval": 30,
           "ping_timeout": 10
       }
   }

Options
~~~~~~~

* ``host`` - Bind address (default: ``0.0.0.0``)
* ``port`` - WebSocket port (default: ``8081``)
* ``max_rooms`` - Maximum concurrent rooms (default: ``100``)
* ``max_participants_per_room`` - Max participants per room (default: ``50``)
* ``ping_interval`` - WebSocket ping interval in seconds (default: ``30``)
* ``ping_timeout`` - Ping timeout in seconds (default: ``10``)

TURN Configuration
------------------

.. code-block:: json

   {
       "turn": {
           "enabled": true,
           "server": "turn:turn.example.com:3478",
           "username": "user",
           "credential": "pass",
           "realm": "example.com"
       }
   }

Options
~~~~~~~

* ``enabled`` - Enable TURN server (default: ``false``)
* ``server`` - TURN server URL (required if enabled)
* ``username`` - TURN username (required if enabled)
* ``credential`` - TURN password (required if enabled)
* ``realm`` - TURN realm (optional)

Runtime Isolation
-----------------

.. code-block:: json

   {
       "runtime_isolation": {
           "mode": "process",
           "lxc_enabled": false,
           "lxc_template": "ubuntu",
           "lxc_path": "/var/lib/lxc"
       }
   }

Options
~~~~~~~

* ``mode`` - Isolation mode: ``process`` or ``lxc`` (default: ``process``)
* ``lxc_enabled`` - Enable LXC containers (default: ``false``)
* ``lxc_template`` - LXC template name (default: ``ubuntu``)
* ``lxc_path`` - LXC container path (default: ``/var/lib/lxc``)

Services Configuration
----------------------

.. code-block:: json

   {
       "services": {
           "autostart": ["echo", "room_ops", "bot_api"],
           "health_check_interval": 30,
           "restart_on_failure": true,
           "max_restart_attempts": 3,
           "restart_delay": 5
       }
   }

Options
~~~~~~~

* ``autostart`` - Services to start automatically (default: ``[]``)
* ``health_check_interval`` - Health check interval in seconds (default: ``30``)
* ``restart_on_failure`` - Auto-restart failed services (default: ``true``)
* ``max_restart_attempts`` - Max restart attempts (default: ``3``)
* ``restart_delay`` - Delay between restarts in seconds (default: ``5``)

Frontend Bridge
---------------

.. code-block:: json

   {
       "frontend_bridge": {
           "enabled": true,
           "event_buffer_size": 1000,
           "event_retention": 3600
       }
   }

Options
~~~~~~~

* ``enabled`` - Enable frontend bridge (default: ``true``)
* ``event_buffer_size`` - Event buffer size (default: ``1000``)
* ``event_retention`` - Event retention in seconds (default: ``3600``)

Voice Configuration
-------------------

.. code-block:: json

   {
       "voice": {
           "sample_rate": 48000,
           "channels": 1,
           "frame_size": 480,
           "opus_bitrate": 32000,
           "opus_complexity": 10,
           "rnnoise_enabled": true
       }
   }

Options
~~~~~~~

* ``sample_rate`` - Audio sample rate in Hz (default: ``48000``)
* ``channels`` - Audio channels (default: ``1`` for mono)
* ``frame_size`` - Frame size in samples (default: ``480``)
* ``opus_bitrate`` - Opus bitrate in bps (default: ``32000``)
* ``opus_complexity`` - Opus complexity 0-10 (default: ``10``)
* ``rnnoise_enabled`` - Enable noise suppression (default: ``true``)

Extensions Configuration
------------------------

.. code-block:: json

   {
       "extensions": {
           "wasm_memory_limit": 10485760,
           "cpu_time_limit_ms": 100,
           "storage_limit": 1048576,
           "max_timers": 10
       }
   }

Options
~~~~~~~

* ``wasm_memory_limit`` - WASM memory limit in bytes (default: ``10485760`` = 10MB)
* ``cpu_time_limit_ms`` - CPU time limit per handler in ms (default: ``100``)
* ``storage_limit`` - Storage limit per extension in bytes (default: ``1048576`` = 1MB)
* ``max_timers`` - Max active timers per extension (default: ``10``)

Database Configuration
----------------------

.. code-block:: json

   {
       "database": {
           "type": "json",
           "path": "/var/lib/daffychat/data"
       }
   }

JSON Backend
~~~~~~~~~~~~

.. code-block:: json

   {
       "database": {
           "type": "json",
           "path": "/var/lib/daffychat/data"
       }
   }

PostgreSQL Backend
~~~~~~~~~~~~~~~~~~

.. code-block:: json

   {
       "database": {
           "type": "postgresql",
           "host": "localhost",
           "port": 5432,
           "database": "daffychat",
           "username": "daffychat",
           "password": "secret",
           "pool_size": 10
       }
   }

Logging Configuration
---------------------

.. code-block:: json

   {
       "logging": {
           "level": "info",
           "format": "json",
           "output": "file",
           "file_path": "/var/log/daffychat/daffychat.log",
           "max_file_size": 104857600,
           "max_files": 10
       }
   }

Options
~~~~~~~

* ``level`` - Log level: ``debug``, ``info``, ``warn``, ``error`` (default: ``info``)
* ``format`` - Log format: ``text`` or ``json`` (default: ``text``)
* ``output`` - Output: ``stdout``, ``stderr``, or ``file`` (default: ``stdout``)
* ``file_path`` - Log file path (required if output is ``file``)
* ``max_file_size`` - Max log file size in bytes (default: ``104857600`` = 100MB)
* ``max_files`` - Max log files to keep (default: ``10``)

Environment Variables
---------------------

Configuration can be overridden with environment variables:

.. code-block:: bash

   # Server
   export DAFFYCHAT_SERVER_HOST=0.0.0.0
   export DAFFYCHAT_SERVER_PORT=8080

   # Signaling
   export DAFFYCHAT_SIGNALING_HOST=0.0.0.0
   export DAFFYCHAT_SIGNALING_PORT=8081

   # TURN
   export DAFFYCHAT_TURN_ENABLED=true
   export DAFFYCHAT_TURN_SERVER=turn:turn.example.com:3478
   export DAFFYCHAT_TURN_USERNAME=user
   export DAFFYCHAT_TURN_CREDENTIAL=pass

   # Logging
   export DAFFYCHAT_LOG_LEVEL=debug

Configuration Validation
------------------------

Validate configuration file:

.. code-block:: bash

   daffychat --config /etc/daffychat/config.json --validate

This checks:

* JSON syntax
* Required fields
* Value ranges
* Type correctness

Example Configurations
----------------------

Development
~~~~~~~~~~~

.. code-block:: json

   {
       "server": {
           "host": "127.0.0.1",
           "port": 8080
       },
       "signaling": {
           "host": "127.0.0.1",
           "port": 8081
       },
       "logging": {
           "level": "debug",
           "output": "stdout"
       }
   }

Production
~~~~~~~~~~

.. code-block:: json

   {
       "server": {
           "host": "0.0.0.0",
           "port": 8080,
           "workers": 8,
           "max_connections": 5000
       },
       "signaling": {
           "host": "0.0.0.0",
           "port": 8081,
           "max_rooms": 500
       },
       "turn": {
           "enabled": true,
           "server": "turn:turn.example.com:3478",
           "username": "user",
           "credential": "pass"
       },
       "database": {
           "type": "postgresql",
           "host": "db.example.com",
           "database": "daffychat"
       },
       "logging": {
           "level": "info",
           "format": "json",
           "output": "file",
           "file_path": "/var/log/daffychat/daffychat.log"
       }
   }

See Also
--------

* :doc:`deployment` - Deployment guide
* :doc:`monitoring` - Monitoring guide
* :doc:`troubleshooting` - Troubleshooting guide
