Server Setup Guide
==================

This guide explains how to deploy DaffyChat frontend and backend on a production server with Socket.IO voice transport enabled.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

DaffyChat consists of three main components:

1. **Signaling Server** - WebSocket signaling for WebRTC connections
2. **Socket.IO Voice Transport** - Browser-friendly voice signaling layer
3. **Frontend** - Static HTML/JS files for the web interface

This guide covers deployment on Linux servers (Ubuntu/Debian, CentOS/RHEL, Arch).

Prerequisites
-------------

System Requirements
~~~~~~~~~~~~~~~~~~~

- Linux server (Ubuntu 20.04+, Debian 11+, CentOS 8+, or Arch Linux)
- 2+ CPU cores
- 2GB+ RAM
- 10GB+ disk space
- Public IP address or domain name
- Ports 80, 443, 7001, 7002 available

Required Software
~~~~~~~~~~~~~~~~~

- CMake 3.20+
- C++20 compiler (GCC 10+, Clang 12+)
- Git
- Web server (Nginx or Apache)
- SSL certificate (Let's Encrypt recommended)

Installation
------------

Step 1: Install Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Ubuntu/Debian:**

.. code-block:: bash

   sudo apt update
   sudo apt install -y build-essential cmake git pkg-config \
     libssl-dev libopus-dev libsrtp2-dev portaudio19-dev \
     libdatachannel-dev nginx certbot python3-certbot-nginx

**CentOS/RHEL:**

.. code-block:: bash

   sudo dnf install -y gcc-c++ cmake git pkgconfig \
     openssl-devel opus-devel libsrtp-devel portaudio-devel \
     nginx certbot python3-certbot-nginx
   
   # Install libdatachannel from source
   git clone https://github.com/paullouisageneau/libdatachannel.git
   cd libdatachannel
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   sudo cmake --build build --target install

**Arch Linux:**

.. code-block:: bash

   sudo pacman -S base-devel cmake git opus libsrtp portaudio \
     libdatachannel nginx certbot certbot-nginx

Step 2: Build DaffyChat
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Clone repository
   git clone https://github.com/yourusername/daffychat.git
   cd daffychat

   # Build with CMake
   cmake -B build -DCMAKE_BUILD_TYPE=Release \
     -DDAFFY_ENABLE_TESTS=OFF \
     -DDAFFY_ENABLE_FRONTEND_ASSETS=ON

   cmake --build build -j$(nproc)

   # Install system-wide
   sudo cmake --install build

Step 3: Configure DaffyChat
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create configuration file at ``/etc/daffychat/config.json``:

.. code-block:: json

   {
     "server": {
       "bind_address": "0.0.0.0",
       "port": 8080,
       "log_level": "info"
     },
     "signaling": {
       "bind_address": "0.0.0.0",
       "port": 7001,
       "max_room_size": 10,
       "reconnect_grace_ms": 30000,
       "ping_interval_ms": 25000,
       "stun_servers": [
         "stun:stun.l.google.com:19302",
         "stun:stun1.l.google.com:19302"
       ],
       "health_endpoint": "/health",
       "debug_rooms_endpoint": "/debug/rooms",
       "turn_credentials_endpoint": "/turn/credentials"
     },
     "turn": {
       "uri": "",
       "username": "",
       "password": "",
       "credential_mode": "static"
     },
     "voice": {
       "preferred_input_device": "",
       "preferred_output_device": "",
       "preferred_capture_sample_rate": 48000,
       "preferred_playback_sample_rate": 48000,
       "preferred_channels": 1,
       "frames_per_buffer": 480,
       "playout_buffer_frames": 960,
       "max_playout_buffer_frames": 4800,
       "enable_noise_suppression": true,
       "enable_metrics": true
     },
     "frontend_bridge": {
       "enabled": true,
       "bridge_endpoint": "/api/bridge"
     }
   }

Step 4: Set Up Systemd Services
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create signaling service at ``/etc/systemd/system/daffychat-signaling.service``:

.. code-block:: ini

   [Unit]
   Description=DaffyChat Signaling Server
   After=network.target

   [Service]
   Type=simple
   User=daffychat
   Group=daffychat
   WorkingDirectory=/opt/daffychat
   ExecStart=/usr/local/bin/daffy-signaling --serve-socketio /etc/daffychat/config.json
   Restart=on-failure
   RestartSec=5
   StandardOutput=journal
   StandardError=journal

   [Install]
   WantedBy=multi-user.target

Create backend service at ``/etc/systemd/system/daffychat-backend.service``:

.. code-block:: ini

   [Unit]
   Description=DaffyChat Backend Server
   After=network.target

   [Service]
   Type=simple
   User=daffychat
   Group=daffychat
   WorkingDirectory=/opt/daffychat
   ExecStart=/usr/local/bin/daffy-backend --serve /etc/daffychat/config.json
   Restart=on-failure
   RestartSec=5
   StandardOutput=journal
   StandardError=journal

   [Install]
   WantedBy=multi-user.target

Create dedicated user:

.. code-block:: bash

   sudo useradd -r -s /bin/false daffychat
   sudo mkdir -p /opt/daffychat
   sudo chown daffychat:daffychat /opt/daffychat

Enable and start services:

.. code-block:: bash

   sudo systemctl daemon-reload
   sudo systemctl enable daffychat-signaling daffychat-backend
   sudo systemctl start daffychat-signaling daffychat-backend

Step 5: Configure Nginx
~~~~~~~~~~~~~~~~~~~~~~~~

Create Nginx configuration at ``/etc/nginx/sites-available/daffychat``:

.. code-block:: nginx

   # HTTP to HTTPS redirect
   server {
       listen 80;
       listen [::]:80;
       server_name your-domain.com;
       
       location /.well-known/acme-challenge/ {
           root /var/www/certbot;
       }
       
       location / {
           return 301 https://$server_name$request_uri;
       }
   }

   # HTTPS server
   server {
       listen 443 ssl http2;
       listen [::]:443 ssl http2;
       server_name your-domain.com;

       # SSL configuration
       ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
       ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;
       ssl_protocols TLSv1.2 TLSv1.3;
       ssl_ciphers HIGH:!aNULL:!MD5;
       ssl_prefer_server_ciphers on;

       # Frontend static files
       root /usr/local/share/daffychat/frontend;
       index index.html;

       # Frontend routes
       location / {
           try_files $uri $uri/ /index.html;
       }

       # Backend API
       location /api/ {
           proxy_pass http://127.0.0.1:8080;
           proxy_http_version 1.1;
           proxy_set_header Upgrade $http_upgrade;
           proxy_set_header Connection 'upgrade';
           proxy_set_header Host $host;
           proxy_cache_bypass $http_upgrade;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_set_header X-Forwarded-Proto $scheme;
       }

       # WebSocket signaling
       location /ws {
           proxy_pass http://127.0.0.1:7001;
           proxy_http_version 1.1;
           proxy_set_header Upgrade $http_upgrade;
           proxy_set_header Connection "Upgrade";
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_read_timeout 86400;
       }

       # Socket.IO voice transport
       location /socket.io/ {
           proxy_pass http://127.0.0.1:7002;
           proxy_http_version 1.1;
           proxy_set_header Upgrade $http_upgrade;
           proxy_set_header Connection "Upgrade";
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_read_timeout 86400;
       }

       # Health check endpoints
       location /health {
           proxy_pass http://127.0.0.1:7001;
           access_log off;
       }

       # Security headers
       add_header X-Frame-Options "SAMEORIGIN" always;
       add_header X-Content-Type-Options "nosniff" always;
       add_header X-XSS-Protection "1; mode=block" always;
       add_header Referrer-Policy "no-referrer-when-downgrade" always;

       # Gzip compression
       gzip on;
       gzip_vary on;
       gzip_min_length 1024;
       gzip_types text/plain text/css text/xml text/javascript 
                  application/x-javascript application/xml+rss 
                  application/json application/javascript;
   }

Enable the site:

.. code-block:: bash

   sudo ln -s /etc/nginx/sites-available/daffychat /etc/nginx/sites-enabled/
   sudo nginx -t
   sudo systemctl reload nginx

Step 6: Obtain SSL Certificate
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   sudo certbot --nginx -d your-domain.com

Follow the prompts to obtain and install the certificate.

Step 7: Configure Firewall
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**UFW (Ubuntu/Debian):**

.. code-block:: bash

   sudo ufw allow 80/tcp
   sudo ufw allow 443/tcp
   sudo ufw allow 7001/tcp
   sudo ufw allow 7002/tcp
   sudo ufw enable

**firewalld (CentOS/RHEL):**

.. code-block:: bash

   sudo firewall-cmd --permanent --add-service=http
   sudo firewall-cmd --permanent --add-service=https
   sudo firewall-cmd --permanent --add-port=7001/tcp
   sudo firewall-cmd --permanent --add-port=7002/tcp
   sudo firewall-cmd --reload

Frontend Configuration
----------------------

Update Frontend URLs
~~~~~~~~~~~~~~~~~~~~

Edit ``/usr/local/share/daffychat/frontend/app/config.js``:

.. code-block:: javascript

   const DAFFYCHAT_CONFIG = {
     signalingUrl: 'wss://your-domain.com/ws',
     socketIOUrl: 'https://your-domain.com',
     apiUrl: 'https://your-domain.com/api',
     stunServers: [
       'stun:stun.l.google.com:19302',
       'stun:stun1.l.google.com:19302'
     ]
   };

TURN Server Setup (Optional)
-----------------------------

For better connectivity behind NATs and firewalls, set up a TURN server.

Install Coturn
~~~~~~~~~~~~~~

.. code-block:: bash

   sudo apt install coturn  # Ubuntu/Debian
   sudo dnf install coturn  # CentOS/RHEL
   sudo pacman -S coturn    # Arch Linux

Configure Coturn
~~~~~~~~~~~~~~~~

Edit ``/etc/turnserver.conf``:

.. code-block:: ini

   listening-port=3478
   tls-listening-port=5349
   listening-ip=0.0.0.0
   relay-ip=YOUR_SERVER_IP
   external-ip=YOUR_SERVER_IP
   
   realm=your-domain.com
   server-name=your-domain.com
   
   lt-cred-mech
   user=daffychat:your-secret-password
   
   cert=/etc/letsencrypt/live/your-domain.com/fullchain.pem
   pkey=/etc/letsencrypt/live/your-domain.com/privkey.pem
   
   no-stdout-log
   log-file=/var/log/turnserver.log
   
   fingerprint
   no-multicast-peers

Enable and start Coturn:

.. code-block:: bash

   sudo systemctl enable coturn
   sudo systemctl start coturn

Update DaffyChat config to use TURN:

.. code-block:: json

   {
     "turn": {
       "uri": "turn:your-domain.com:3478",
       "username": "daffychat",
       "password": "your-secret-password",
       "credential_mode": "static"
     }
   }

Restart services:

.. code-block:: bash

   sudo systemctl restart daffychat-signaling daffychat-backend

Monitoring and Maintenance
---------------------------

View Logs
~~~~~~~~~

.. code-block:: bash

   # Signaling server logs
   sudo journalctl -u daffychat-signaling -f

   # Backend server logs
   sudo journalctl -u daffychat-backend -f

   # Nginx logs
   sudo tail -f /var/log/nginx/access.log
   sudo tail -f /var/log/nginx/error.log

Health Checks
~~~~~~~~~~~~~

.. code-block:: bash

   # Check signaling server
   curl https://your-domain.com/health

   # Check services status
   sudo systemctl status daffychat-signaling
   sudo systemctl status daffychat-backend

Backup Configuration
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Backup config
   sudo tar czf daffychat-config-$(date +%Y%m%d).tar.gz \
     /etc/daffychat/ \
     /etc/nginx/sites-available/daffychat \
     /etc/systemd/system/daffychat-*.service

Updates
~~~~~~~

.. code-block:: bash

   cd daffychat
   git pull
   cmake --build build
   sudo cmake --install build
   sudo systemctl restart daffychat-signaling daffychat-backend

Troubleshooting
---------------

Service Won't Start
~~~~~~~~~~~~~~~~~~~

Check logs for errors:

.. code-block:: bash

   sudo journalctl -u daffychat-signaling -n 50
   sudo journalctl -u daffychat-backend -n 50

Common issues:

- Port already in use: Check with ``sudo netstat -tlnp | grep :7001``
- Permission denied: Ensure ``daffychat`` user has proper permissions
- Config file errors: Validate JSON syntax

WebSocket Connection Fails
~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Check firewall rules
2. Verify Nginx proxy configuration
3. Test direct connection: ``curl http://localhost:7001/health``
4. Check SSL certificate validity

No Audio in Calls
~~~~~~~~~~~~~~~~~

1. Verify microphone permissions in browser
2. Check STUN/TURN server configuration
3. Test with ``voice-demo.html``
4. Review browser console for WebRTC errors

Socket.IO Connection Issues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Verify Socket.IO server is running on port 7002
2. Check Nginx proxy for ``/socket.io/`` location
3. Test direct connection: ``curl http://localhost:7002/socket.io/``
4. Review browser console for connection errors

Performance Optimization
------------------------

System Tuning
~~~~~~~~~~~~~

Increase file descriptor limits in ``/etc/security/limits.conf``:

.. code-block:: text

   daffychat soft nofile 65536
   daffychat hard nofile 65536

Nginx Optimization
~~~~~~~~~~~~~~~~~~

Add to Nginx configuration:

.. code-block:: nginx

   worker_processes auto;
   worker_rlimit_nofile 65536;
   
   events {
       worker_connections 4096;
       use epoll;
   }

Database Caching
~~~~~~~~~~~~~~~~

If using persistent storage, enable Redis caching:

.. code-block:: bash

   sudo apt install redis-server
   sudo systemctl enable redis-server
   sudo systemctl start redis-server

Security Hardening
------------------

1. **Enable fail2ban** for SSH protection
2. **Use strong passwords** for TURN server
3. **Regularly update** system packages
4. **Monitor logs** for suspicious activity
5. **Implement rate limiting** in Nginx
6. **Use Content Security Policy** headers

Example CSP header:

.. code-block:: nginx

   add_header Content-Security-Policy "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self' wss: https:;" always;

Scaling
-------

For high-traffic deployments:

1. **Load balancing**: Use multiple backend instances behind HAProxy or Nginx
2. **Database replication**: Set up master-slave PostgreSQL replication
3. **CDN**: Serve static assets via CloudFlare or similar
4. **Monitoring**: Deploy Prometheus + Grafana for metrics
5. **Horizontal scaling**: Add more servers and use sticky sessions

Support
-------

- Documentation: https://daffychat.readthedocs.io
- GitHub Issues: https://github.com/yourusername/daffychat/issues
- Community Chat: Join a DaffyChat room!

License
-------

DaffyChat is open source software. See LICENSE file for details.
