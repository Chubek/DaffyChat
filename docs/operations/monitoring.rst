Monitoring
==========

Guide to monitoring DaffyChat in production.

Health Checks
-------------

Service Health
~~~~~~~~~~~~~~

Check main service health:

.. code-block:: bash

   curl http://localhost:8080/health

Response:

.. code-block:: json

   {
       "status": "healthy",
       "version": "1.0.0",
       "uptime_seconds": 3600
   }

Daemon Manager Health
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   daffydmd status

Individual Service Health
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   daffydmd status echo
   daffydmd status room_ops
   daffydmd status bot_api

Metrics
-------

Prometheus Metrics
~~~~~~~~~~~~~~~~~~

DaffyChat exposes Prometheus metrics at ``/metrics``:

.. code-block:: bash

   curl http://localhost:8080/metrics

Key metrics:

* ``daffychat_rooms_total`` - Total number of rooms
* ``daffychat_rooms_active`` - Active rooms
* ``daffychat_participants_total`` - Total participants
* ``daffychat_voice_streams_active`` - Active voice streams
* ``daffychat_http_requests_total`` - HTTP request count
* ``daffychat_http_request_duration_seconds`` - Request latency
* ``daffychat_service_status`` - Service status (0=stopped, 1=active)

Prometheus Configuration
~~~~~~~~~~~~~~~~~~~~~~~~

Add to ``prometheus.yml``:

.. code-block:: yaml

   scrape_configs:
     - job_name: 'daffychat'
       static_configs:
         - targets: ['localhost:8080']
       metrics_path: '/metrics'
       scrape_interval: 15s

Grafana Dashboard
~~~~~~~~~~~~~~~~~

Import the DaffyChat dashboard:

.. code-block:: bash

   curl -o daffychat-dashboard.json \
       https://grafana.com/api/dashboards/12345/revisions/1/download

Key panels:

* Active rooms over time
* Participant count
* Voice stream count
* HTTP request rate
* Request latency (p50, p95, p99)
* Service status

Logging
-------

Log Locations
~~~~~~~~~~~~~

* **Main service:** ``/var/log/daffychat/daffychat.log``
* **Daemon manager:** ``/var/log/daffychat/daffydmd.log``
* **Individual services:** ``/var/log/daffychat/<service>.log``

Systemd Logs
~~~~~~~~~~~~

.. code-block:: bash

   # Main service
   sudo journalctl -u daffychat -f

   # Daemon manager
   sudo journalctl -u daffydmd -f

   # Last 100 lines
   sudo journalctl -u daffychat -n 100

   # Since specific time
   sudo journalctl -u daffychat --since "2025-01-01 00:00:00"

Log Levels
~~~~~~~~~~

* ``DEBUG`` - Detailed debugging information
* ``INFO`` - General informational messages
* ``WARN`` - Warning messages
* ``ERROR`` - Error messages

Configure log level in ``config.json``:

.. code-block:: json

   {
       "logging": {
           "level": "info"
       }
   }

Log Aggregation
~~~~~~~~~~~~~~~

**ELK Stack (Elasticsearch, Logstash, Kibana):**

Configure Filebeat to ship logs:

.. code-block:: yaml

   filebeat.inputs:
     - type: log
       enabled: true
       paths:
         - /var/log/daffychat/*.log
       fields:
         service: daffychat

   output.logstash:
     hosts: ["logstash:5044"]

**Loki:**

Configure Promtail:

.. code-block:: yaml

   clients:
     - url: http://loki:3100/loki/api/v1/push

   scrape_configs:
     - job_name: daffychat
       static_configs:
         - targets:
             - localhost
           labels:
             job: daffychat
             __path__: /var/log/daffychat/*.log

Alerting
--------

Prometheus Alerts
~~~~~~~~~~~~~~~~~

Create ``daffychat-alerts.yml``:

.. code-block:: yaml

   groups:
     - name: daffychat
       rules:
         - alert: ServiceDown
           expr: daffychat_service_status == 0
           for: 1m
           labels:
             severity: critical
           annotations:
             summary: "DaffyChat service {{ $labels.service }} is down"

         - alert: HighErrorRate
           expr: rate(daffychat_http_requests_total{status=~"5.."}[5m]) > 0.05
           for: 5m
           labels:
             severity: warning
           annotations:
             summary: "High error rate detected"

         - alert: HighLatency
           expr: histogram_quantile(0.95, daffychat_http_request_duration_seconds) > 1
           for: 5m
           labels:
             severity: warning
           annotations:
             summary: "High request latency (p95 > 1s)"

Alertmanager Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   route:
     receiver: 'team-email'
     group_by: ['alertname', 'severity']
     group_wait: 10s
     group_interval: 10s
     repeat_interval: 12h

   receivers:
     - name: 'team-email'
       email_configs:
         - to: 'team@example.com'
           from: 'alerts@example.com'
           smarthost: 'smtp.example.com:587'

Performance Monitoring
----------------------

CPU Usage
~~~~~~~~~

.. code-block:: bash

   # Overall CPU
   top -p $(pgrep -d',' daffychat)

   # Per-service CPU
   ps aux | grep daffy

Memory Usage
~~~~~~~~~~~~

.. code-block:: bash

   # Overall memory
   ps -o pid,user,%mem,rss,cmd -p $(pgrep daffychat)

   # Detailed memory map
   pmap $(pgrep daffychat)

Network Usage
~~~~~~~~~~~~~

.. code-block:: bash

   # Network connections
   netstat -anp | grep daffychat

   # Bandwidth usage
   iftop -f "port 8080 or port 8081"

Disk I/O
~~~~~~~~

.. code-block:: bash

   # I/O stats
   iostat -x 1

   # Per-process I/O
   iotop -p $(pgrep daffychat)

Tracing
-------

Distributed Tracing
~~~~~~~~~~~~~~~~~~~

DaffyChat supports OpenTelemetry for distributed tracing.

Configure in ``config.json``:

.. code-block:: json

   {
       "tracing": {
           "enabled": true,
           "exporter": "jaeger",
           "endpoint": "http://jaeger:14268/api/traces"
       }
   }

View traces in Jaeger UI:

.. code-block:: text

   http://jaeger:16686

Profiling
---------

CPU Profiling
~~~~~~~~~~~~~

.. code-block:: bash

   # Attach perf
   sudo perf record -p $(pgrep daffychat) -g -- sleep 30
   sudo perf report

Memory Profiling
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Valgrind massif
   valgrind --tool=massif ./daffychat

   # Analyze
   ms_print massif.out.*

Dashboards
----------

System Dashboard
~~~~~~~~~~~~~~~~

Key metrics to monitor:

* CPU usage (overall and per-service)
* Memory usage (overall and per-service)
* Disk usage and I/O
* Network bandwidth
* System load average

Application Dashboard
~~~~~~~~~~~~~~~~~~~~~

Key metrics to monitor:

* Active rooms
* Total participants
* Active voice streams
* HTTP request rate
* Request latency (p50, p95, p99)
* Error rate
* Service status

Business Dashboard
~~~~~~~~~~~~~~~~~~

Key metrics to monitor:

* Daily active users
* Peak concurrent users
* Average session duration
* Room creation rate
* Message rate

Automated Monitoring
--------------------

Health Check Script
~~~~~~~~~~~~~~~~~~~

Create ``/usr/local/bin/daffychat-healthcheck.sh``:

.. code-block:: bash

   #!/bin/bash
   
   # Check main service
   if ! curl -sf http://localhost:8080/health > /dev/null; then
       echo "Main service unhealthy"
       exit 1
   fi
   
   # Check daemon manager
   if ! daffydmd status > /dev/null; then
       echo "Daemon manager unhealthy"
       exit 1
   fi
   
   echo "All services healthy"
   exit 0

Add to cron:

.. code-block:: bash

   */5 * * * * /usr/local/bin/daffychat-healthcheck.sh || mail -s "DaffyChat Health Check Failed" admin@example.com

Capacity Planning
-----------------

Resource Estimation
~~~~~~~~~~~~~~~~~~~

Per 100 concurrent users:

* **CPU:** 1-2 cores
* **RAM:** 2GB
* **Bandwidth:** 3.2 Mbps (32 kbps per stream)
* **Disk I/O:** Minimal (mostly logs)

Scaling Triggers
~~~~~~~~~~~~~~~~

Scale up when:

* CPU usage > 70% sustained
* Memory usage > 80%
* Request latency p95 > 500ms
* Active rooms > 80% of max_rooms

See Also
--------

* :doc:`deployment` - Deployment guide
* :doc:`configuration` - Configuration reference
* :doc:`troubleshooting` - Troubleshooting guide
