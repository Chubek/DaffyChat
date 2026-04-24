C++ API Reference
=================

This section documents the C++ APIs for DaffyChat core libraries.

.. note::
   C++ API documentation is generated from source code using Doxygen and Breathe.
   Run ``doxygen`` in the project root to generate the full API reference.

Core Libraries
--------------

IPC Transport
~~~~~~~~~~~~~

.. doxygenclass:: daffy::NngTransport
   :members:
   :undoc-members:

Room Runtime
~~~~~~~~~~~~

.. doxygenclass:: daffy::RoomRuntime
   :members:
   :undoc-members:

Daemon Manager
~~~~~~~~~~~~~~

.. doxygenclass:: daffy::DaemonManager
   :members:
   :undoc-members:

Voice Components
----------------

Audio Pipeline
~~~~~~~~~~~~~~

.. doxygenclass:: daffy::AudioPipeline
   :members:
   :undoc-members:

Opus Codec
~~~~~~~~~~

.. doxygenclass:: daffy::OpusCodec
   :members:
   :undoc-members:

PortAudio Streams
~~~~~~~~~~~~~~~~~

.. doxygenclass:: daffy::PortAudioStreams
   :members:
   :undoc-members:

RTP/SRTP
~~~~~~~~

.. doxygenclass:: daffy::RtpSrtp
   :members:
   :undoc-members:

Service Interfaces
------------------

Generated service interfaces from DSSL specifications.

Echo Service
~~~~~~~~~~~~

.. code-block:: cpp

   namespace daffy {
       class EchoService {
       public:
           virtual EchoReply Echo(const std::string& message,
                                  const std::string& sender) = 0;
       };
   }

Room Operations Service
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   namespace daffy {
       class RoomOpsService {
       public:
           virtual JoinReply Join(const std::string& user) = 0;
           virtual LeaveReply Leave(const std::string& user) = 0;
       };
   }

Bot API Service
~~~~~~~~~~~~~~~

.. code-block:: cpp

   namespace daffy {
       class BotApiService {
       public:
           virtual BotRecord RegisterBot(const std::string& display_name,
                                         const std::vector<std::string>& capabilities,
                                         const std::vector<std::string>& room_scope) = 0;
           virtual BotRecord GetBot(const std::string& bot_id) = 0;
           virtual std::vector<BotRecord> ListBots(bool enabled,
                                                   const std::string& room_id,
                                                   const std::string& capability) = 0;
           // ... more methods
       };
   }

Utility Classes
---------------

JSON Utilities
~~~~~~~~~~~~~~

.. doxygennamespace:: daffy::json
   :members:

Event Bus
~~~~~~~~~

.. doxygenclass:: daffy::EventBus
   :members:
   :undoc-members:

Configuration
~~~~~~~~~~~~~

.. doxygenclass:: daffy::Config
   :members:
   :undoc-members:

Building API Documentation
--------------------------

Generate full C++ API documentation:

.. code-block:: bash

   # Install Doxygen
   sudo apt-get install doxygen graphviz

   # Generate documentation
   cd /path/to/daffychat
   doxygen

   # View documentation
   firefox build/html/index.html

The generated documentation includes:

* Class hierarchies
* Member function documentation
* Code examples
* Call graphs
* Dependency diagrams

See Also
--------

* :doc:`../python/index` - Python API reference
* :doc:`../../dssl/index` - DSSL service definitions
* :doc:`../../development/building` - Building from source
