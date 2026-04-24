Troubleshooting
===============

Common issues and solutions for DaffyChat.

Service Issues
--------------

Service Won't Start
~~~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: bash

   $ daffydmd start echo
   Error: Failed to start service 'echo'

**Diagnosis:**

.. code-block:: bash

   # Check service logs
   tail -f /var/log/daffychat/echo.log

   # Check systemd status
   sudo systemctl status daffychat

   # Check for port conflicts
   sudo netstat -tulpn | grep 8080

**Solutions:**

1. Check configuration file syntax
2. Verify IPC socket path is writable
3. Ensure no port conflicts
4. Check file permissions

Service Crashes Repeatedly
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: bash

   $ daffydmd status echo
   Status: failed (restarting)

**Diagnosis:**

.. code-block:: bash

   # Check crash logs
   sudo journalctl -u daffychat -n 100

   # Check core dumps
   coredumpctl list

**Solutions:**

1. Review error logs for exceptions
2. Check resource limits (ulimit)
3. Verify dependencies are installed
4. Update to latest version

IPC Socket Permission Denied
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: text

   Error: Permission denied: /tmp/daffy-echo.ipc

**Solutions:**

.. code-block:: bash

   # Fix socket permissions
   sudo chmod 666 /tmp/daffy-*.ipc

   # Or run with correct user
   sudo -u daffychat daffydmd start echo

Voice Issues
------------

No Audio
~~~~~~~~

**Symptoms:**

* Participants can't hear each other
* Audio indicator not showing

**Diagnosis:**

.. code-block:: bash

   # Test audio devices
   aplay -l  # List playback devices
   arecord -l  # List capture devices

   # Test PortAudio
   ./build/voice-loopback-test

**Solutions:**

1. Check audio device permissions
2. Verify PortAudio configuration
3. Test with loopback harness
4. Check firewall rules for WebRTC

Poor Audio Quality
~~~~~~~~~~~~~~~~~~

**Symptoms:**

* Choppy or distorted audio
* High latency
* Echo or feedback

**Diagnosis:**

.. code-block:: bash

   # Check CPU usage
   top -p $(pgrep daffychat)

   # Check network latency
   ping <peer-ip>

**Solutions:**

1. Reduce Opus bitrate in config
2. Enable RNNoise for noise suppression
3. Check network bandwidth
4. Reduce CPU load

WebRTC Connection Fails
~~~~~~~~~~~~~~~~~~~~~~~

**Symptoms:**

* Peers can't establish connection
* Stuck on "Connecting..."

**Diagnosis:**

.. code-block:: bash

   # Check signaling server
   curl http://localhost:8081/health

   # Check TURN server
   turnutils_uclient -v turn.example.com

**Solutions:**

1. Verify signaling server is running
2. Configure TURN server for NAT traversal
3. Check firewall rules
4. Verify ICE candidates are exchanged

Network Issues
--------------

Connection Timeout
~~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: text

   Error: Connection timeout

**Diagnosis:**

.. code-block:: bash

   # Test connectivity
   curl -v http://localhost:8080/health

   # Check listening ports
   sudo netstat -tulpn | grep daffychat

**Solutions:**

1. Verify service is running
2. Check firewall rules
3. Verify bind address in config
4. Check reverse proxy configuration

High Latency
~~~~~~~~~~~~

**Symptoms:**

* Slow API responses
* Delayed messages

**Diagnosis:**

.. code-block:: bash

   # Check request latency
   curl -w "@curl-format.txt" http://localhost:8080/api/v1/rooms

   # Monitor metrics
   curl http://localhost:8080/metrics | grep duration

**Solutions:**

1. Check database performance
2. Increase worker threads
3. Enable caching
4. Optimize queries

Database Issues
---------------

Connection Failed
~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: text

   Error: Failed to connect to database

**Diagnosis:**

.. code-block:: bash

   # Test database connection
   psql -h localhost -U daffychat -d daffychat

**Solutions:**

1. Verify database is running
2. Check credentials in config
3. Verify network connectivity
4. Check PostgreSQL logs

Slow Queries
~~~~~~~~~~~~

**Symptoms:**

* Slow room listing
* Delayed participant updates

**Diagnosis:**

.. code-block:: sql

   -- PostgreSQL slow query log
   SELECT * FROM pg_stat_statements 
   ORDER BY mean_exec_time DESC 
   LIMIT 10;

**Solutions:**

1. Add database indexes
2. Optimize queries
3. Increase connection pool size
4. Consider read replicas

Extension Issues
----------------

Extension Won't Load
~~~~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: text

   Error: Failed to load extension 'greeter.wasm'

**Diagnosis:**

.. code-block:: bash

   # Validate WASM module
   wasm-validate greeter.wasm

   # Check extension logs
   tail -f /var/log/daffychat/extensions.log

**Solutions:**

1. Verify WASM module is valid
2. Check file permissions
3. Verify extension size < limit
4. Check for compilation errors

Extension Timeout
~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: text

   Warning: Extension handler exceeded time limit

**Solutions:**

1. Optimize extension code
2. Increase cpu_time_limit_ms in config
3. Reduce handler complexity
4. Use async operations

Memory Issues
-------------

Out of Memory
~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: text

   Error: Cannot allocate memory

**Diagnosis:**

.. code-block:: bash

   # Check memory usage
   free -h

   # Check per-process memory
   ps aux --sort=-%mem | head

**Solutions:**

1. Increase system RAM
2. Reduce max_connections
3. Reduce extension memory limits
4. Check for memory leaks

Memory Leak
~~~~~~~~~~~

**Symptoms:**

* Memory usage grows over time
* Eventually crashes with OOM

**Diagnosis:**

.. code-block:: bash

   # Monitor memory over time
   watch -n 1 'ps aux | grep daffychat'

   # Use valgrind
   valgrind --leak-check=full ./daffychat

**Solutions:**

1. Update to latest version
2. Report bug with valgrind output
3. Restart service periodically
4. Enable memory profiling

Configuration Issues
--------------------

Invalid Configuration
~~~~~~~~~~~~~~~~~~~~~

**Symptoms:**

.. code-block:: text

   Error: Invalid configuration

**Diagnosis:**

.. code-block:: bash

   # Validate configuration
   daffychat --config /etc/daffychat/config.json --validate

**Solutions:**

1. Check JSON syntax
2. Verify required fields
3. Check value ranges
4. Use sample config as template

Configuration Not Applied
~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptoms:**

* Changes to config.json not taking effect

**Solutions:**

1. Restart service after config changes
2. Verify config file location
3. Check for environment variable overrides
4. Verify file permissions

Performance Issues
------------------

High CPU Usage
~~~~~~~~~~~~~~

**Symptoms:**

* CPU usage > 80%
* Slow response times

**Diagnosis:**

.. code-block:: bash

   # Profile CPU usage
   sudo perf top -p $(pgrep daffychat)

**Solutions:**

1. Reduce Opus complexity
2. Disable RNNoise if not needed
3. Increase worker threads
4. Scale horizontally

High Memory Usage
~~~~~~~~~~~~~~~~~

**Symptoms:**

* Memory usage growing
* Swap usage increasing

**Diagnosis:**

.. code-block:: bash

   # Check memory breakdown
   pmap -x $(pgrep daffychat)

**Solutions:**

1. Reduce max_connections
2. Reduce event_buffer_size
3. Reduce extension memory limits
4. Add more RAM

Debugging Tools
---------------

Enable Debug Logging
~~~~~~~~~~~~~~~~~~~~

.. code-block:: json

   {
       "logging": {
           "level": "debug"
       }
   }

Restart service to apply.

Packet Capture
~~~~~~~~~~~~~~

.. code-block:: bash

   # Capture HTTP traffic
   sudo tcpdump -i any -w daffychat.pcap port 8080

   # Analyze with Wireshark
   wireshark daffychat.pcap

System Tracing
~~~~~~~~~~~~~~

.. code-block:: bash

   # Trace system calls
   sudo strace -p $(pgrep daffychat) -f

   # Trace library calls
   sudo ltrace -p $(pgrep daffychat)

Core Dumps
~~~~~~~~~~

Enable core dumps:

.. code-block:: bash

   ulimit -c unlimited
   echo "/tmp/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern

Analyze core dump:

.. code-block:: bash

   gdb ./daffychat /tmp/core.daffychat.12345

Getting Help
------------

Check Documentation
~~~~~~~~~~~~~~~~~~~

* :doc:`../index` - Main documentation
* :doc:`configuration` - Configuration reference
* :doc:`deployment` - Deployment guide

Community Support
~~~~~~~~~~~~~~~~~

* GitHub Issues: https://github.com/yourusername/daffychat/issues
* Discord: https://discord.gg/daffychat
* Forum: https://forum.daffychat.io

Bug Reports
~~~~~~~~~~~

When reporting bugs, include:

1. DaffyChat version
2. Operating system and version
3. Configuration file (sanitized)
4. Relevant log excerpts
5. Steps to reproduce
6. Expected vs actual behavior

.. code-block:: bash

   # Collect diagnostic info
   daffychat --version
   uname -a
   daffydmd status
   tail -n 100 /var/log/daffychat/daffychat.log

See Also
--------

* :doc:`deployment` - Deployment guide
* :doc:`configuration` - Configuration reference
* :doc:`monitoring` - Monitoring guide
