DSSL (DaffyChat Service Definition Language)
=============================================

.. include:: README.md
   :parser: myst_parser.sphinx_

Overview
--------

DSSL is a declarative IDL (Interface Definition Language) for defining microservices in the DaffyChat ecosystem. It generates type-safe C++ service implementations with automatic IPC transport, serialization, and daemon lifecycle management.

Quick Example
-------------

.. code-block:: dssl

   /// Simple echo service
   service echo 1.0.0;

   struct EchoRequest {
       message: string;
       sender: string;
   }

   struct EchoReply {
       message: string;
       echoed: bool;
   }

   rpc Echo(message: string, sender: string) returns EchoReply;

Documentation Sections
----------------------

* :doc:`language-reference` - Complete DSSL syntax and semantics
* :doc:`getting-started` - Tutorial for creating your first service
* :doc:`code-generation` - How DSSL compiles to C++ services
* :doc:`best-practices` - Design patterns and conventions
* :doc:`toolchain` - CLI tools and workflow
* :doc:`examples/index` - Real-world service definitions

Key Features
------------

* **Type-safe RPC definitions** - Strongly typed request/response contracts
* **Automatic code generation** - Produces complete C++ service skeletons
* **Built-in IPC transport** - Uses NNG for inter-process communication
* **Daemon lifecycle management** - Integrates with daffydmd for service orchestration
* **Multi-RPC services** - Single service can expose multiple endpoints
* **Semantic versioning** - Services declare version compatibility

Architecture
------------

DSSL services run as independent daemons managed by ``daffydmd``. Each service:

1. Listens on a Unix domain socket (``/tmp/daffy-<service>.ipc``)
2. Implements generated C++ interfaces
3. Handles JSON-RPC 2.0 requests over NNG transport
4. Registers with the daemon manager for lifecycle control

Toolchain
---------

* ``dssl-bindgen`` - Core code generator (C++ binary)
* ``toolchain/dssl-bindgen.py`` - User-friendly CLI wrapper
* ``toolchain/dssl-init.py`` - Service scaffolding tool
* ``toolchain/install-service.py`` - Daemon installation helper

Next Steps
----------

* Read the :doc:`language-reference` for complete syntax
* Follow the :doc:`getting-started` tutorial
* Explore :doc:`examples/index` for real service definitions
