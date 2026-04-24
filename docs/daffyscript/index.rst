Daffyscript (WASM Extensions)
==============================

.. include:: README.md
   :parser: myst_parser.sphinx_

Overview
--------

Daffyscript is a JavaScript-like language that compiles to WebAssembly for creating server-hosted room extensions in DaffyChat. It provides a safe, sandboxed environment for custom room logic, bot behaviors, and event handlers.

Quick Example
-------------

.. code-block:: javascript

   // Simple greeting module
   export function onMessage(event) {
       if (event.text.startsWith("!hello")) {
           return {
               type: "message",
               text: "Hello, " + event.sender + "!"
           };
       }
   }

Documentation Sections
----------------------

* :doc:`language-reference` - Complete Daffyscript syntax
* :doc:`modules` - Module system and exports
* :doc:`programs` - Standalone programs
* :doc:`recipes` - Room recipes and configuration
* :doc:`compiler` - Compilation and toolchain
* :doc:`examples` - Real-world Daffyscript code

Key Features
------------

* **JavaScript-like syntax** - Familiar to web developers
* **WebAssembly compilation** - Fast, portable execution
* **Server-hosted** - Extensions run on the server, not in browsers
* **Event-driven** - React to room events with handlers
* **Safe sandboxing** - Isolated execution environment
* **Room-scoped** - Extensions are tied to specific rooms

Extension Types
---------------

Modules
~~~~~~~

Reusable libraries that export functions and constants:

.. code-block:: javascript

   export const VERSION = "1.0.0";
   
   export function greet(name) {
       return "Hello, " + name;
   }

Programs
~~~~~~~~

Standalone scripts that execute in response to events:

.. code-block:: javascript

   function main() {
       console.log("Program started");
       // Program logic
   }

Recipes
~~~~~~~

Room configuration and initialization scripts:

.. code-block:: javascript

   {
       "name": "My Room",
       "max_participants": 10,
       "extensions": ["greeter", "moderator"]
   }

Runtime Environment
-------------------

Daffyscript extensions run in a WebAssembly runtime with:

* **Event handlers** - ``onMessage``, ``onJoin``, ``onLeave``, etc.
* **Room API** - Access room state and participants
* **Bot API** - Send messages and perform actions
* **Storage API** - Persistent key-value storage

Compilation
-----------

Compile Daffyscript to WASM:

.. code-block:: bash

   daffyscript compile my-extension.dfy -o my-extension.wasm

Load in a room:

.. code-block:: bash

   # Via CLI
   daffychat load-extension my-room my-extension.wasm

   # Via frontend extension manager

Next Steps
----------

* Read the :doc:`language-reference` for complete syntax
* Explore :doc:`modules` to understand the module system
* Check :doc:`examples` for real-world code
* Learn about :doc:`compiler` options and workflow
