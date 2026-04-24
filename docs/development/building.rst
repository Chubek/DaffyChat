Building from Source
====================

Complete guide to building DaffyChat from source.

Prerequisites
-------------

See :doc:`../installation` for detailed prerequisites.

Quick summary:

.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get install -y \
       build-essential cmake git python3 \
       libportaudio2 libopus-dev libsrtp2-dev \
       libnng-dev nlohmann-json3-dev

Clone Repository
----------------

.. code-block:: bash

   git clone https://github.com/yourusername/daffychat.git
   cd daffychat
   git submodule update --init --recursive

Build Systems
-------------

DaffyChat supports three build systems:

CMake (Recommended)
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   mkdir build
   cd build
   cmake ..
   cmake --build . -j$(nproc)

**Build options:**

.. code-block:: bash

   cmake .. \
       -DCMAKE_BUILD_TYPE=Release \
       -DBUILD_TESTS=ON \
       -DBUILD_DOCS=ON \
       -DENABLE_LXC=OFF

Meson
~~~~~

.. code-block:: bash

   meson setup build
   meson compile -C build

**Build options:**

.. code-block:: bash

   meson setup build \
       --buildtype=release \
       -Dtests=true \
       -Ddocs=true \
       -Dlxc=false

Ninja
~~~~~

.. code-block:: bash

   ninja -f build.ninja

Build Targets
-------------

All Targets
~~~~~~~~~~~

.. code-block:: bash

   # CMake
   cmake --build build

   # Meson
   meson compile -C build

   # Ninja
   ninja

Specific Targets
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # CMake
   cmake --build build --target daffychat
   cmake --build build --target daffydmd
   cmake --build build --target dssl-bindgen

   # Meson
   meson compile -C build daffychat
   meson compile -C build daffydmd

Tests
~~~~~

.. code-block:: bash

   # CMake
   cmake --build build --target test

   # Meson
   meson test -C build

Documentation
~~~~~~~~~~~~~

.. code-block:: bash

   # CMake
   cmake --build build --target docs

   # Meson
   meson compile -C build docs

Build Configuration
-------------------

Debug Build
~~~~~~~~~~~

.. code-block:: bash

   # CMake
   cmake -DCMAKE_BUILD_TYPE=Debug ..

   # Meson
   meson setup build --buildtype=debug

Release Build
~~~~~~~~~~~~~

.. code-block:: bash

   # CMake
   cmake -DCMAKE_BUILD_TYPE=Release ..

   # Meson
   meson setup build --buildtype=release

With Optimizations
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # CMake
   cmake -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-O3 -march=native" ..

   # Meson
   meson setup build --buildtype=release \
         -Dcpp_args="-O3 -march=native"

Build Options
-------------

CMake Options
~~~~~~~~~~~~~

* ``BUILD_TESTS`` - Build test suite (default: ON)
* ``BUILD_DOCS`` - Build documentation (default: OFF)
* ``ENABLE_LXC`` - Enable LXC support (default: OFF)
* ``ENABLE_ASAN`` - Enable AddressSanitizer (default: OFF)
* ``ENABLE_TSAN`` - Enable ThreadSanitizer (default: OFF)
* ``ENABLE_UBSAN`` - Enable UndefinedBehaviorSanitizer (default: OFF)

Meson Options
~~~~~~~~~~~~~

* ``tests`` - Build test suite (default: true)
* ``docs`` - Build documentation (default: false)
* ``lxc`` - Enable LXC support (default: false)
* ``asan`` - Enable AddressSanitizer (default: false)
* ``tsan`` - Enable ThreadSanitizer (default: false)

Dependencies
------------

System Dependencies
~~~~~~~~~~~~~~~~~~~

Required:

* PortAudio
* Opus
* libsrtp
* NNG
* nlohmann-json

Optional:

* LXC (for container isolation)
* Doxygen (for C++ docs)
* Sphinx (for this documentation)

Third-party Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~

Included as submodules in ``third_party/``:

* libdatachannel
* rnnoise
* wasm-micro-runtime
* uWebSockets

Cross-compilation
-----------------

ARM64
~~~~~

.. code-block:: bash

   cmake .. \
       -DCMAKE_TOOLCHAIN_FILE=cmake/arm64-toolchain.cmake \
       -DCMAKE_BUILD_TYPE=Release

ARMv7
~~~~~

.. code-block:: bash

   cmake .. \
       -DCMAKE_TOOLCHAIN_FILE=cmake/armv7-toolchain.cmake \
       -DCMAKE_BUILD_TYPE=Release

Static Linking
--------------

.. code-block:: bash

   cmake .. \
       -DBUILD_SHARED_LIBS=OFF \
       -DCMAKE_EXE_LINKER_FLAGS="-static"

Sanitizers
----------

AddressSanitizer
~~~~~~~~~~~~~~~~

.. code-block:: bash

   cmake .. -DENABLE_ASAN=ON
   cmake --build .
   ./build/daffychat

ThreadSanitizer
~~~~~~~~~~~~~~~

.. code-block:: bash

   cmake .. -DENABLE_TSAN=ON
   cmake --build .
   ./build/daffychat

UndefinedBehaviorSanitizer
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   cmake .. -DENABLE_UBSAN=ON
   cmake --build .
   ./build/daffychat

Code Coverage
-------------

.. code-block:: bash

   cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
   cmake --build .
   ctest
   gcovr -r .. --html --html-details -o coverage.html

Troubleshooting
---------------

Missing Dependencies
~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   CMake Error: Could not find libopus

**Solution:**

.. code-block:: bash

   sudo apt-get install libopus-dev

Submodule Issues
~~~~~~~~~~~~~~~~

.. code-block:: text

   fatal: No url found for submodule path 'third_party/...'

**Solution:**

.. code-block:: bash

   git submodule update --init --recursive

Build Failures
~~~~~~~~~~~~~~

.. code-block:: bash

   # Clean build
   rm -rf build
   mkdir build
   cd build
   cmake ..
   cmake --build .

Incremental Builds
------------------

After making changes:

.. code-block:: bash

   # CMake
   cmake --build build

   # Meson
   meson compile -C build

   # Ninja
   ninja

Only changed files and their dependents are rebuilt.

Installation
------------

System-wide
~~~~~~~~~~~

.. code-block:: bash

   cd build
   sudo cmake --install .

Local
~~~~~

.. code-block:: bash

   cd build
   cmake --install . --prefix=$HOME/.local

Custom Prefix
~~~~~~~~~~~~~

.. code-block:: bash

   cmake .. -DCMAKE_INSTALL_PREFIX=/opt/daffychat
   cmake --build .
   sudo cmake --install .

See Also
--------

* :doc:`../installation` - Installation guide
* :doc:`testing` - Testing guide
* :doc:`contributing` - Contributing guide
