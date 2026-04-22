# DaffyChat Deployment

This document describes the repository’s current deployment path for operators.

## Status

Deployment support is partially implemented. The repository already contains:

- installable binaries
- packaging helpers in `toolchain/`
- a sample configuration file
- a deployment helper script
- a systemd unit template for the future daemon manager

The missing pieces are a full `daffydmd` implementation, production-ready services, and hardened room isolation.

## Package Build Flow

Use the existing helper to build distributable artifacts from an install tree or staged build output.

Typical flow:

```sh
cmake -S . -B build/cmake -G Ninja
cmake --build build/cmake
cmake --install build/cmake --prefix /tmp/daffychat-stage/usr
python3 toolchain/package_artifact.py --stage-dir /tmp/daffychat-stage --format tgz --output-dir dist
```

The helper also contains support for `.deb`, `.rpm`, and Pacman-style packages when the required host tools are installed.

## Service Model

Today, the concrete runnable processes are:

- `daffy-backend`
- `daffy-signaling`

The future model adds `daffydmd` as a daemon supervisor and IPC broker for generated DSSL services. A bootstrap unit file is provided now so deployment workflows can be shaped before the daemon implementation lands.

## Recommended Host Layout

- Install binaries under `/usr/local` or `/opt/daffychat`.
- Store mutable configuration under `/etc/daffychat/`.
- Store room workspace state under `/var/lib/daffychat/rooms/`.
- Store logs under your platform’s journal or log aggregation pipeline.

## Using `scripts/deploy.sh`

The repository now includes `scripts/deploy.sh` for bootstrap deployment. It:

- installs the CMake build into a chosen prefix
- copies the sample configuration into `/etc/daffychat/` if missing
- installs `daffydmd.service` into the systemd unit directory when requested
- prints the next commands needed to enable the service

Example:

```sh
sudo scripts/deploy.sh build/cmake /opt/daffychat
```

## Installing The Future `daffydmd` Unit

The provided unit file intentionally points at an expected future binary path. After deployment:

```sh
sudo cp scripts/daffydmd.service /etc/systemd/system/daffydmd.service
sudo systemctl daemon-reload
```

Enablement should wait until an actual `daffydmd` binary is installed.

## Production Gaps

Do not treat the current repository state as a finished production release. The major remaining items are:

- implement `daffydmd`
- add concrete room services under `service/`
- harden LXC-based room isolation
- expand extension examples and operational docs
- validate packaging on target distributions
