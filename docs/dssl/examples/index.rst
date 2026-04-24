DSSL Examples
=============

.. include:: README.md
   :parser: myst_parser.sphinx_

Example Services
----------------

Basic Services
~~~~~~~~~~~~~~

.. toctree::
   :maxdepth: 1

   echo
   health

Multi-RPC Services
~~~~~~~~~~~~~~~~~~

.. toctree::
   :maxdepth: 1

   room_ops
   bot_api

Implementation Notes
--------------------

.. include:: implementation-notes.md
   :parser: myst_parser.sphinx_

Service Patterns
----------------

Single RPC Pattern
~~~~~~~~~~~~~~~~~~

Services with one method are ideal for simple request/response operations:

* Health checks
* Echo/ping services
* Simple transformations

See :doc:`echo` and :doc:`health` for examples.

Multi-RPC Pattern
~~~~~~~~~~~~~~~~~

Services with multiple methods handle related operations:

* CRUD operations
* Room management
* User operations

See :doc:`room_ops` for a simple multi-RPC example.

Complex Service Pattern
~~~~~~~~~~~~~~~~~~~~~~~

Services with many methods and complex state:

* Bot integration APIs
* Event streaming
* Authentication and authorization

See :doc:`bot_api` for a comprehensive example.

Learning Path
-------------

1. Start with :doc:`echo` - Understand basic structure
2. Study :doc:`room_ops` - Learn multi-RPC patterns
3. Explore :doc:`bot_api` - See complex service design
4. Review implementation notes for each example

Running Examples
----------------

All examples are production services in DaffyChat:

.. code-block:: bash

   # Generate code
   ./toolchain/dssl-bindgen.py --target cpp --out-dir ./gen services/specs/echo.dssl

   # Build service
   cd build && cmake --build . -j2

   # Run service
   ./build/echo-service
