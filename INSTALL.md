# DaffyChat Installation

This document covers local bootstrap installation for developers and operators.

## Prerequisites

DaffyChat currently builds as a native C++ project with vendored third-party dependencies in the repository. The main host requirements are:

- CMake 3.20+
- A C++20 compiler (`clang++` or `g++`)
- Python 3
- OpenSSL development headers
- `pkg-config`
- Audio and media dependencies required by the voice stack when enabled by the host environment

Package names vary by distribution, but a typical Debian or Ubuntu bootstrap looks like:

```sh
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config python3 libssl-dev
```

## Configure

From the repository root:

```sh
cmake -S . -B build/cmake -G Ninja
```

Useful options:

- `-DDAFFY_ENABLE_TESTS=ON` builds the current test targets.
- `-DDAFFY_ENABLE_FRONTEND_ASSETS=ON` installs the bundled HTML and CSS assets.
- `-DDAFFY_ENABLE_WERROR=ON` turns warnings into errors.
- `-DDAFFY_SANITIZER=address` enables a supported sanitizer on Clang/GCC.

## Build

```sh
cmake --build build/cmake
```

Primary binaries produced by the bootstrap build:

- `daffy-backend`
- `daffy-signaling`
- `dssl-bindgen`
- `dssl-docstrip`
- `dssl-docgen`
- `daffyscript`

## Test

Run the CTest suite from the build directory:

```sh
ctest --test-dir build/cmake --output-on-failure
```

Some tests depend on optional host capabilities or libraries; start with the targeted test matching the area you changed when iterating.

## Install Into a Prefix

To stage an install under a custom prefix:

```sh
cmake -S . -B build/cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/opt/daffychat
cmake --build build/cmake
cmake --install build/cmake
```

Installed content includes binaries, sample configuration, bundled frontend assets, deployment scripts, and operator documentation.

## Installed Layout

The bootstrap installation currently uses this layout under the selected prefix:

- `bin/` for executables
- `lib/` for libraries and archives
- `share/daffychat/config/` for example configuration
- `share/daffychat/frontend/` for bundled frontend assets
- `share/daffychat/docs/` for operator and extension documentation
- `share/daffychat/scripts/` for deployment helpers and service unit files

## First Run

1. Copy `config/daffychat.example.json` to an environment-specific file.
2. Adjust `server`, `signaling`, `turn`, and `runtime_isolation` settings.
3. Start `daffy-signaling` and `daffy-backend` with the chosen configuration.
4. If you are evaluating the future daemon-manager shape, install `scripts/daffydmd.service` as described in `DEPLOY.md`.

## Current Limitations

This repository still contains placeholder implementations in several subsystems. Installation works best today as a developer bootstrap and packaging baseline rather than as a complete production deployment.
