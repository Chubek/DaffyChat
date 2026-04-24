Python API Reference
====================

This section documents the Python toolchain scripts and utilities.

Toolchain Scripts
-----------------

dssl-bindgen.py
~~~~~~~~~~~~~~~

.. automodule:: dssl-bindgen
   :members:
   :undoc-members:
   :show-inheritance:

dssl-init.py
~~~~~~~~~~~~

.. automodule:: dssl-init
   :members:
   :undoc-members:
   :show-inheritance:

plugin-init.py
~~~~~~~~~~~~~~

.. automodule:: plugin-init
   :members:
   :undoc-members:
   :show-inheritance:

install-service.py
~~~~~~~~~~~~~~~~~~

.. automodule:: install-service
   :members:
   :undoc-members:
   :show-inheritance:

dfc-mkrecipe.py
~~~~~~~~~~~~~~~

.. automodule:: dfc-mkrecipe
   :members:
   :undoc-members:
   :show-inheritance:

package_artifact.py
~~~~~~~~~~~~~~~~~~~

.. automodule:: package_artifact
   :members:
   :undoc-members:
   :show-inheritance:

Usage Examples
--------------

DSSL Code Generation
~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   from pathlib import Path
   import subprocess

   # Generate C++ service from DSSL spec
   spec_path = Path("services/specs/echo.dssl")
   out_dir = Path("generated")
   
   subprocess.run([
       "python3", "toolchain/dssl-bindgen.py",
       "--target", "cpp",
       "--out-dir", str(out_dir),
       str(spec_path)
   ])

Service Scaffolding
~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import subprocess

   # Create new service
   subprocess.run([
       "python3", "toolchain/dssl-init.py",
       "--name", "greeter",
       "--version", "1.0.0",
       "--rpc", "Greet"
   ])

Package Creation
~~~~~~~~~~~~~~~~

.. code-block:: python

   import subprocess

   # Build DEB package
   subprocess.run([
       "python3", "toolchain/package_artifact.py",
       "--format", "deb"
   ])

See Also
--------

* :doc:`../cpp/index` - C++ API reference
* :doc:`../../dssl/toolchain` - DSSL toolchain guide
* :doc:`../../development/building` - Building from source
