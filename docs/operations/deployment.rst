Deployment
==========

This guide covers deploying DaffyChat in production environments.

Deployment Options
------------------

Standalone Server
~~~~~~~~~~~~~~~~~

Single-server deployment for small to medium deployments:

* All services on one machine
* Suitable for up to 100 concurrent users
* Simplest to manage

Distributed Deployment
~~~~~~~~~~~~~~~~~~~~~~

Multi-server deployment for larger scale:

* Services distributed across multiple machines
* Load balancing for frontend and API
* Suitable for 100+ concurrent users

Container Deployment
~~~~~~~~~~~~~~~~~~~~

Docker/Kubernetes deployment:

* Containerized services
* Orchestration with Kubernetes
* Auto-scaling and high availability

System Requirements
-------------------

Minimum Requirements
~~~~~~~~~~~~~~~~~~~~

* **CPU:** 2 cores
* **RAM:** 4GB
* **Disk:** 10GB
* **Network:** 10 Mbps

Recommended Requirements
~~~~~~~~~~~~~~~~~~~~~~~~

* **CPU:** 4+ cores
* **RAM:** 8GB+
* **Disk:** 50GB SSD
* **Network:** 100 Mbps+

Per-User Resource Estimates
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **CPU:** ~10% per voice participant
* **RAM:** ~200KB per voice participant
* **Bandwidth:** ~32 kbps per voice stream

Installation
------------

Package Installation
~~~~~~~~~~~~~~~~~~~~

**Debian/Ubuntu:**

.. code-block:: bash

   wget https://releases.daffychat.io/daffychat_1.0.0_amd64.deb
   sudo dpkg -i daffychat_1.0.0_amd64.deb
   sudo apt-get install -f

**RHEL/CentOS:**

.. code-block:: bash

   wget https://releases.daffychat.io/daffychat-1.0.0.x86_64.rpm
   sudo rpm -i daffychat-1.0.0.x86_64.rpm

**From Source:**

.. code-block:: bash

   git clone https://github.com/yourusername/daffychat.git
   cd daffychat
   mkdir build && cd build
   cmake ..
   cmake --build . -j$(nproc)
   sudo cmake --install .

Configuration
-------------

System Configuration
~~~~~~~~~~~~~~~~~~~~

Create configuration directory:

.. code-block:: bash

   sudo mkdir -p /etc/daffychat
   sudo cp config/daffychat.json.sample /etc/daffychat/config.json
   sudo chown -R daffychat:daffychat /etc/daffychat

Edit ``/etc/daffychat/config.json``:

.. code-block:: json

   {
       "server": {
           "host": "0.0.0.0",
           "port": 8080,
           "workers": 4
       },
       "signaling": {
           "host": "0.0.0.0",
           "port": 8081
       },
       "turn": {
           "enabled": true,
           "server": "turn:turn.example.com:3478",
           "username": "user",
           "credential": "pass"
       },
       "services": {
           "autostart": ["echo", "room_ops", "bot_api"],
           "health_check_interval": 30
       }
   }

Systemd Service
~~~~~~~~~~~~~~~

Create ``/etc/systemd/system/daffychat.service``:

.. code-block:: ini

   [Unit]
   Description=DaffyChat Server
   After=network.target

   [Service]
   Type=simple
   User=daffychat
   Group=daffychat
   WorkingDirectory=/var/lib/daffychat
   ExecStart=/usr/local/bin/daffychat --config /etc/daffychat/config.json
   Restart=on-failure
   RestartSec=5

   [Install]
   WantedBy=multi-user.target

Enable and start:

.. code-block:: bash

   sudo systemctl daemon-reload
   sudo systemctl enable daffychat
   sudo systemctl start daffychat

Daemon Manager Service
~~~~~~~~~~~~~~~~~~~~~~

Create ``/etc/systemd/system/daffydmd.service``:

.. code-block:: ini

   [Unit]
   Description=DaffyChat Daemon Manager
   After=network.target

   [Service]
   Type=simple
   User=daffychat
   Group=daffychat
   ExecStart=/usr/local/bin/daffydmd
   Restart=on-failure
   RestartSec=5

   [Install]
   WantedBy=multi-user.target

Enable and start:

.. code-block:: bash

   sudo systemctl enable daffydmd
   sudo systemctl start daffydmd

Reverse Proxy
-------------

Nginx Configuration
~~~~~~~~~~~~~~~~~~~

Create ``/etc/nginx/sites-available/daffychat``:

.. code-block:: nginx

   upstream daffychat_backend {
       server 127.0.0.1:8080;
   }

   upstream daffychat_signaling {
       server 127.0.0.1:8081;
   }

   server {
       listen 80;
       server_name chat.example.com;

       # Redirect to HTTPS
       return 301 https://$server_name$request_uri;
   }

   server {
       listen 443 ssl http2;
       server_name chat.example.com;

       ssl_certificate /etc/letsencrypt/live/chat.example.com/fullchain.pem;
       ssl_certificate_key /etc/letsencrypt/live/chat.example.com/privkey.pem;

       # API
       location /api/ {
           proxy_pass http://daffychat_backend;
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_set_header X-Forwarded-Proto $scheme;
       }

       # Signaling WebSocket
       location /signaling {
           proxy_pass http://daffychat_signaling;
           proxy_http_version 1.1;
           proxy_set_header Upgrade $http_upgrade;
           proxy_set_header Connection "upgrade";
           proxy_set_header Host $host;
       }

       # Frontend
       location / {
           root /var/www/daffychat;
           try_files $uri $uri/ /index.html;
       }
   }

Enable site:

.. code-block:: bash

   sudo ln -s /etc/nginx/sites-available/daffychat /etc/nginx/sites-enabled/
   sudo nginx -t
   sudo systemctl reload nginx

SSL/TLS
-------

Let's Encrypt
~~~~~~~~~~~~~

.. code-block:: bash

   sudo apt-get install certbot python3-certbot-nginx
   sudo certbot --nginx -d chat.example.com

Auto-renewal:

.. code-block:: bash

   sudo systemctl enable certbot.timer
   sudo systemctl start certbot.timer

Database
--------

DaffyChat uses JSON files for persistence by default. For production, consider:

PostgreSQL Backend
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   sudo apt-get install postgresql
   sudo -u postgres createdb daffychat
   sudo -u postgres createuser daffychat

Configure in ``config.json``:

.. code-block:: json

   {
       "database": {
           "type": "postgresql",
           "host": "localhost",
           "port": 5432,
           "database": "daffychat",
           "username": "daffychat",
           "password": "secret"
       }
   }

Monitoring
----------

Health Checks
~~~~~~~~~~~~~

.. code-block:: bash

   # Check main service
   curl http://localhost:8080/health

   # Check daemon manager
   daffydmd status

Logs
~~~~

.. code-block:: bash

   # Service logs
   sudo journalctl -u daffychat -f

   # Daemon manager logs
   sudo journalctl -u daffydmd -f

   # Individual service logs
   tail -f /var/log/daffy/echo.log

Metrics
~~~~~~~

Prometheus metrics endpoint:

.. code-block:: text

   http://localhost:8080/metrics

Backup
------

Configuration Backup
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   sudo tar -czf daffychat-config-$(date +%Y%m%d).tar.gz /etc/daffychat

Data Backup
~~~~~~~~~~~

.. code-block:: bash

   sudo tar -czf daffychat-data-$(date +%Y%m%d).tar.gz /var/lib/daffychat

Automated Backups
~~~~~~~~~~~~~~~~~

Create ``/etc/cron.daily/daffychat-backup``:

.. code-block:: bash

   #!/bin/bash
   tar -czf /backup/daffychat-$(date +%Y%m%d).tar.gz \
       /etc/daffychat \
       /var/lib/daffychat
   
   # Keep last 7 days
   find /backup -name "daffychat-*.tar.gz" -mtime +7 -delete

Security
--------

Firewall
~~~~~~~~

.. code-block:: bash

   # Allow HTTP/HTTPS
   sudo ufw allow 80/tcp
   sudo ufw allow 443/tcp

   # Allow signaling (if not behind reverse proxy)
   sudo ufw allow 8081/tcp

   # Allow TURN
   sudo ufw allow 3478/tcp
   sudo ufw allow 3478/udp

User Isolation
~~~~~~~~~~~~~~

Run services as dedicated user:

.. code-block:: bash

   sudo useradd -r -s /bin/false daffychat
   sudo chown -R daffychat:daffychat /var/lib/daffychat

Updates
-------

Package Updates
~~~~~~~~~~~~~~~

.. code-block:: bash

   # Debian/Ubuntu
   sudo apt-get update
   sudo apt-get upgrade daffychat

   # RHEL/CentOS
   sudo yum update daffychat

Rolling Updates
~~~~~~~~~~~~~~~

For zero-downtime updates:

1. Deploy new version to standby server
2. Switch load balancer to new server
3. Update old server
4. Switch back or keep new server active

See Also
--------

* :doc:`configuration` - Configuration reference
* :doc:`monitoring` - Monitoring guide
* :doc:`troubleshooting` - Troubleshooting guide
