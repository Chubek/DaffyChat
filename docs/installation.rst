Installation
============

This guide covers building and installing DaffyChat from source.

Prerequisites
-------------

System Requirements
~~~~~~~~~~~~~~~~~~~

* Linux (Ubuntu 20.04+, Debian 11+, or equivalent)
* 4GB RAM minimum
* 2GB disk space for build artifacts

Build Tools
~~~~~~~~~~~

* GCC 10+ or Clang 12+
* CMake 3.20+ (or Meson 0.60+)
* Python 3.8+
* Git

Dependencies
~~~~~~~~~~~~

**Required libraries:**

.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get install -y \
       build-essential \
       cmake \
       git \
       python3 \
       libportaudio2 \
       libopus-dev \
       libsrtp2-dev \
       libnng-dev \
       nlohmann-json3-dev

**Optional dependencies:**

* Doxygen (for C++ API documentation)
* Sphinx (for this documentation)
* Node.js 18+ (for frontend development)

Building from Source
--------------------

Clone the Repository
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   git clone https://github.com/yourusername/daffychat.git
   cd daffychat
   git submodule update --init --recursive

Build with CMake
~~~~~~~~~~~~~~~~

.. code-block:: bash

   mkdir build
   cd build
   cmake ..
   cmake --build . -j$(nproc)

Build with Meson
~~~~~~~~~~~~~~~~

.. code-block:: bash

   meson setup build
   meson compile -C build

Build with Ninja
~~~~~~~~~~~~~~~~

.. code-block:: bash

   ninja -f build.ninja

Running Tests
-------------

.. code-block:: bash

   cd build
   ctest --output-on-failure

Installation
------------

System-wide Installation
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   cd build
   sudo cmake --install .

This installs:

* Binaries to ``/usr/local/bin/``
* Libraries to ``/usr/local/lib/``
* Headers to ``/usr/local/include/daffy/``
* Services to ``/usr/local/lib/daffy/services/``

Local Installation
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   cd build
   cmake --install . --prefix=$HOME/.local

Package Installation
~~~~~~~~~~~~~~~~~~~~

Build distribution packages:

.. code-block:: bash

   # Debian/Ubuntu package
   python3 toolchain/package_artifact.py --format deb

   # RPM package
   python3 toolchain/package_artifact.py --format rpm

   # Tarball
   python3 toolchain/package_artifact.py --format tar.gz

Install the package:

.. code-block:: bash

   # Debian/Ubuntu
   sudo dpkg -i daffychat_1.0.0_amd64.deb

   # RPM-based
   sudo rpm -i daffychat-1.0.0.x86_64.rpm

Verifying Installation
----------------------

Check that binaries are installed:

.. code-block:: bash

   daffychat --version
   daffydmd --version
   dssl-bindgen --version

Start the daemon manager:

.. code-block:: bash

   daffydmd start

Check service status:

.. code-block:: bash

   daffydmd status

Frontend Setup
--------------

Install Node.js dependencies:

.. code-block:: bash

   cd frontend
   npm install

Start development server:

.. code-block:: bash

   npm run dev

Build for production:

.. code-block:: bash

   npm run build

Configuration
-------------

Copy the sample configuration:

.. code-block:: bash

   cp config/daffychat.json.sample ~/.config/daffychat/config.json

Edit the configuration file to match your environment. See :doc:`operations/configuration` for details.

Next Steps
----------

* Follow the :doc:`quickstart` guide
* Read :doc:`architecture/overview` to understand the system
* Create your first service with :doc:`dssl/getting-started`
