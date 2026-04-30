#!/usr/bin/env python3
"""
DaffyChat package artifact builder.

New flags vs the original:
  --linking    DYNAMIC | STATIC | PACK  (default DYNAMIC)
               PACK: also collect vendored .so files from <build-dir> and
               bundle them under usr/share/daffychat/lib.
  --pack-frontend / --no-pack-frontend  (default: pack)
               When --no-pack-frontend the frontend/ tree is omitted from
               the package (useful for server-only packages).
  --client-only
               Only ship client-side binaries (daffyscript, dssl-* tools).
               Package name gets a -client suffix.
"""
import argparse
import bz2
import gzip
import os
import pathlib
import shutil
import subprocess
import tarfile
import tempfile
import time
import zlib

# ---------------------------------------------------------------------------
# Server-side binaries
# ---------------------------------------------------------------------------
SERVER_BINARIES = [
    ("daffydmd",      "usr/bin/daffydmd"),
    ("daffy-backend", "usr/bin/daffy-backend"),
    ("daffy-signaling", "usr/bin/daffy-signaling"),
]

# ---------------------------------------------------------------------------
# Client-side binaries (always included unless --client-only, in which case
# these are the ONLY binaries)
# ---------------------------------------------------------------------------
CLIENT_BINARIES = [
    ("dssl-bindgen", "usr/bin/dssl-bindgen"),
    ("dssl-docstrip", "usr/bin/dssl-docstrip"),
    ("dssl-docgen", "usr/bin/dssl-docgen"),
    ("daffyscript", "usr/bin/daffyscript"),
]

# ---------------------------------------------------------------------------
# Data files that are always included
# ---------------------------------------------------------------------------
COMMON_DATA_FILES = [
    # Config examples
    ("config/daffychat.example.json",
     "usr/share/daffychat/config/daffychat.example.json"),
    # Scripts
    ("scripts/os-service.pl",       "usr/share/daffychat/scripts/os-service.pl"),
    ("scripts/post-install.sh",     "usr/share/daffychat/scripts/post-install.sh"),
    ("scripts/linking-service.sh",  "usr/share/daffychat/scripts/linking-service.sh"),
    ("scripts/install-frontend.sh", "usr/share/daffychat/scripts/install-frontend.sh"),
    ("scripts/stdext-helper.sh",    "usr/share/daffychat/scripts/stdext-helper.sh"),
    # Docs
    ("README.md",       "usr/share/daffychat/docs/README.md"),
    ("INSTALL.md",      "usr/share/daffychat/docs/INSTALL.md"),
    ("GUIDE.md",        "usr/share/daffychat/docs/GUIDE.md"),
    ("DEPLOY.md",       "usr/share/daffychat/docs/DEPLOY.md"),
    ("EXTENSIBILITY.md","usr/share/daffychat/docs/EXTENSIBILITY.md"),
    ("RECIPES.md",      "usr/share/daffychat/docs/RECIPES.md"),
]

# ---------------------------------------------------------------------------
# Server-only data files
# ---------------------------------------------------------------------------
SERVER_DATA_FILES = [
    ("scripts/daffydmd.service",
     "usr/share/daffychat/scripts/daffydmd.service"),
    ("scripts/daffybackend.service",
     "usr/share/daffychat/scripts/daffybackend.service"),
    ("scripts/daffysignaling.service",
     "usr/share/daffychat/scripts/daffysignaling.service"),
    ("config/daffychat.example.json",
     "etc/daffychat/daffychat.json"),
]

# ---------------------------------------------------------------------------
# Frontend assets (omitted when --no-pack-frontend)
# ---------------------------------------------------------------------------
FRONTEND_FILES = [
    ("frontend/index.html",   "usr/share/daffychat/frontend/index.html"),
    ("frontend/room.html",    "usr/share/daffychat/frontend/room.html"),
    ("frontend/guide.html",   "usr/share/daffychat/frontend/guide.html"),
    ("frontend/dynerror.html","usr/share/daffychat/frontend/dynerror.html"),
    ("frontend/bridge.js",    "usr/share/daffychat/frontend/bridge.js"),
    ("frontend/app/styles/daffy.css",
     "usr/share/daffychat/frontend/app/styles/daffy.css"),
    ("frontend/app/state/store.js",
     "usr/share/daffychat/frontend/app/state/store.js"),
    ("frontend/app/api/signaling.js",
     "usr/share/daffychat/frontend/app/api/signaling.js"),
    ("frontend/app/api/extension-manager.js",
     "usr/share/daffychat/frontend/app/api/extension-manager.js"),
    ("frontend/app/hooks/bridge-hooks.js",
     "usr/share/daffychat/frontend/app/hooks/bridge-hooks.js"),
    ("frontend/app/components/message-renderer.js",
     "usr/share/daffychat/frontend/app/components/message-renderer.js"),
    ("frontend/app/components/theme-toggle.js",
     "usr/share/daffychat/frontend/app/components/theme-toggle.js"),
    ("frontend/app/components/extension-panel.js",
     "usr/share/daffychat/frontend/app/components/extension-panel.js"),
    ("frontend/lib/animate.min.css",
     "usr/share/daffychat/frontend/lib/animate.min.css"),
    ("frontend/lib/quote.js",
     "usr/share/daffychat/frontend/lib/quote.js"),
    ("frontend/lib/wasm-runtime.js",
     "usr/share/daffychat/frontend/lib/wasm-runtime.js"),
    ("frontend/lib/alpine.js",
     "usr/share/daffychat/frontend/lib/alpine.js"),
    ("frontend/lib/markdown-it.min.js",
     "usr/share/daffychat/frontend/lib/markdown-it.min.js"),
    ("frontend/resources/boxicons.min.css",
     "usr/share/daffychat/frontend/resources/boxicons.min.css"),
    ("frontend/resources/box-icons.list",
     "usr/share/daffychat/frontend/resources/box-icons.list"),
    ("frontend/resources/quotes.json",
     "usr/share/daffychat/frontend/resources/quotes.json"),
    ("frontend/resources/error-messages.json",
     "usr/share/daffychat/frontend/resources/error-messages.json"),
]


def parse_args():
    parser = argparse.ArgumentParser(description="Build DaffyChat package artifacts from an existing build directory.")
    parser.add_argument("--format", choices=["deb", "rpm", "pacman", "tgz", "tarball", "gz", "bz2", "zlib"], required=True)
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--release", default="1")
    parser.add_argument("--stamp")
    parser.add_argument("--linking", choices=["DYNAMIC", "STATIC", "PACK"],
                        default="DYNAMIC",
                        help="Linking mode used at build time (default: DYNAMIC)")
    parser.add_argument("--pack-frontend", action="store_true", default=True,
                        help="Bundle frontend assets in the package (default: on)")
    parser.add_argument("--no-pack-frontend", action="store_false", dest="pack_frontend",
                        help="Exclude frontend assets from the package")
    parser.add_argument("--client-only", action="store_true", default=False,
                        help="Build a client-only package (daffyscript + tools)")
    parser.add_argument("--with-stdext", action="store_true", default=False,
                        help="Include and install stdext standard extensions")
    parser.add_argument("--no-stdext", action="store_false", dest="with_stdext",
                        help="Exclude stdext standard extensions (default)")
    parser.add_argument("--with-coturn", action="store_true", default=False,
                        help="Include and install vendored coturn from third_party/")
    parser.add_argument("--no-coturn", action="store_false", dest="with_coturn",
                        help="Exclude vendored coturn (default)")
    return parser.parse_args()


def require_tool(name):
    tool = shutil.which(name)
    if not tool:
        raise SystemExit(f"required packaging tool not found: {name}")
    return tool


def detect_arch():
    machine = os.uname().machine
    deb_map = {"x86_64": "amd64", "aarch64": "arm64"}
    return {"native": machine, "deb": deb_map.get(machine, machine), "rpm": machine, "pacman": machine}


def _copy_file(src, dst, *, required=True):
    """Copy src → dst; if src is missing and required=True, raises SystemExit."""
    if not src.exists():
        if required:
            raise SystemExit(f"expected source asset is missing: {src}")
        return  # optional file – silently skip
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def install_tree(source_dir, build_dir, stage_dir, *,
                 linking="DYNAMIC", pack_frontend=True, client_only=False,
                 install_stdext=False, install_coturn=False):
    source_dir = pathlib.Path(source_dir)
    build_dir  = pathlib.Path(build_dir)
    stage_dir  = pathlib.Path(stage_dir)

    # -- Binaries -----------------------------------------------------------
    binaries = CLIENT_BINARIES if client_only else SERVER_BINARIES + CLIENT_BINARIES
    for src_name, dst_rel in binaries:
        src = build_dir / src_name
        _copy_file(src, stage_dir / dst_rel)
        # Make executables actually executable
        (stage_dir / dst_rel).chmod(0o755)

    # -- Common data files --------------------------------------------------
    for src_rel, dst_rel in COMMON_DATA_FILES:
        src = source_dir / src_rel
        _copy_file(src, stage_dir / dst_rel, required=False)
        # Mark shell scripts executable
        if dst_rel.endswith(".sh") or dst_rel.endswith(".pl"):
            dst = stage_dir / dst_rel
            if dst.exists():
                dst.chmod(0o755)

    # -- Server-only data files ---------------------------------------------
    if not client_only:
        config_format = os.environ.get("DAFFY_CONFIG_FORMAT", "json").lower()
        for src_rel, dst_rel in SERVER_DATA_FILES:
            # Skip the wrong config format's entry
            if dst_rel.endswith("daffychat.json") and config_format == "conf":
                continue
            src = source_dir / src_rel
            _copy_file(src, stage_dir / dst_rel, required=False)

        # Optional conf-format config
        if config_format == "conf":
            conf_src = source_dir / "config/daffychat.example.conf"
            if conf_src.exists():
                dst = stage_dir / "etc/daffychat/daffychat.conf"
                _copy_file(conf_src, dst)

    # -- Frontend assets ----------------------------------------------------
    if pack_frontend:
        for src_rel, dst_rel in FRONTEND_FILES:
            src = source_dir / src_rel
            _copy_file(src, stage_dir / dst_rel, required=False)

    # -- Standard extensions (stdext/) -------------------------------------
    if install_stdext:
        stdext_src = source_dir / "stdext"
        stdext_dst = stage_dir / "usr/share/daffychat/stdext"
        if stdext_src.exists():
            if stdext_dst.exists():
                shutil.rmtree(stdext_dst)
            shutil.copytree(stdext_src, stdext_dst)
        else:
            raise SystemExit(f"--with-stdext requested but stdext/ not found at: {stdext_src}")

    # -- Vendored coturn ----------------------------------------------------
    if install_coturn:
        coturn_src = source_dir / "third_party" / "coturn"
        coturn_dst = stage_dir / "usr/share/daffychat/third_party/coturn"
        if coturn_src.exists():
            if coturn_dst.exists():
                shutil.rmtree(coturn_dst)
            shutil.copytree(coturn_src, coturn_dst)
        else:
            raise SystemExit(f"--with-coturn requested but third_party/coturn not found at: {coturn_src}")

    # -- PACK mode: collect vendored shared libraries -----------------------
    if linking == "PACK":
        pack_lib_dst = stage_dir / "usr/share/daffychat/lib"
        pack_lib_dst.mkdir(parents=True, exist_ok=True)
        # Gather all .so* files produced under third_party build outputs
        tp_build = build_dir / "third_party"
        patterns = ["libnng.so*", "libnng-*.so*"]
        found = False
        for pat in patterns:
            for lib in tp_build.rglob(pat) if tp_build.exists() else []:
                dst = pack_lib_dst / lib.name
                if not dst.exists():
                    shutil.copy2(lib, dst)
                    found = True
        if not found:
            # Fallback: search anywhere under build_dir
            for lib in build_dir.rglob("libnng.so*"):
                dst = pack_lib_dst / lib.name
                if not dst.exists():
                    shutil.copy2(lib, dst)

        # Write ld.so.conf.d snippet
        ldconf_dir = stage_dir / "etc/ld.so.conf.d"
        ldconf_dir.mkdir(parents=True, exist_ok=True)
        (ldconf_dir / "daffychat.conf").write_text(
            "/usr/share/daffychat/lib\n", encoding="utf-8")


def packaged_paths(stage_dir):
    paths = []
    for root, dirs, files in os.walk(stage_dir):
        dirs.sort(); files.sort()
        root_path = pathlib.Path(root)
        for filename in files:
            path = root_path / filename
            paths.append("/" + str(path.relative_to(stage_dir)))
    return paths


def _write_postinst(control_dir: pathlib.Path, *,
                    pack_frontend: bool, client_only: bool, linking: str,
                    install_stdext: bool = False):
    """Write DEBIAN/postinst that runs post-install.sh and (if PACK) ldconfig."""
    lines = [
        "#!/bin/sh",
        "set -e",
        "",
        "DAFFY_DATA=/usr/share/daffychat",
        "DAFFY_SCRIPTS=$DAFFY_DATA/scripts",
        "",
    ]

    lines += [
        "# Run DaffyChat post-install helper",
        'DAFFY_PREFIX=/usr/local \\',
        f'DAFFY_CLIENT_ONLY={"1" if client_only else "0"} \\',
        f'DAFFY_PACK_FRONTEND={"1" if pack_frontend else "0"} \\',
        f'DAFFY_INSTALL_STDEXT={"1" if install_stdext else "0"} \\',
        '  sh "$DAFFY_SCRIPTS/post-install.sh"',
        "",
    ]

    if linking == "PACK":
        lines += [
            "# Refresh shared-library cache for packed vendored libs",
            "ldconfig 2>/dev/null || true",
            "",
        ]

    lines.append("")
    postinst_path = control_dir / "postinst"
    postinst_path.write_text("\n".join(lines), encoding="utf-8")
    postinst_path.chmod(0o755)


def _write_conffiles(control_dir: pathlib.Path, stage_dir: pathlib.Path):
    """Write DEBIAN/conffiles listing every file under etc/."""
    conf_files = []
    for root, _, files in os.walk(pathlib.Path(stage_dir) / "etc"):
        for fname in files:
            rel = pathlib.Path(root) / fname
            conf_files.append("/" + str(rel.relative_to(stage_dir)))
    if conf_files:
        (control_dir / "conffiles").write_text(
            "\n".join(sorted(conf_files)) + "\n", encoding="utf-8")


def build_deb(stage_dir, output_dir, version, release, arch, *,
              linking="DYNAMIC", pack_frontend=True, client_only=False,
              install_stdext=False):
    dpkg_deb = require_tool("dpkg-deb")
    control_dir = pathlib.Path(stage_dir) / "DEBIAN"
    control_dir.mkdir(parents=True, exist_ok=True)
    installed_size_kb = 0
    for root, _, files in os.walk(stage_dir):
        for filename in files:
            path = pathlib.Path(root) / filename
            if "DEBIAN" not in path.parts:
                installed_size_kb += (path.stat().st_size + 1023) // 1024

    pkg_name = "daffychat-client" if client_only else "daffychat-server"
    control_dir.joinpath("control").write_text("\n".join([
        f"Package: {pkg_name}",
        f"Version: {version}-{release}",
        "Section: net",
        "Priority: optional",
        f"Architecture: {arch['deb']}",
        "Maintainer: DaffyChat <maintainers@daffychat.local>",
        f"Installed-Size: {max(installed_size_kb, 1)}",
        "Homepage: https://github.com/Chubek/DaffyChat",
        "Description: Native-first peer-to-peer voice chat" +
            (" (client)" if client_only else " (server)"),
        "",
    ]), encoding="utf-8")

    _write_postinst(control_dir,
                    pack_frontend=pack_frontend,
                    client_only=client_only,
                    linking=linking,
                    install_stdext=install_stdext)
    _write_conffiles(control_dir, stage_dir)

    suffix = "client" if client_only else "server"
    output_path = pathlib.Path(output_dir) / \
        f"daffychat-{suffix}_{version}-{release}_{arch['deb']}.deb"
    pathlib.Path(output_dir).mkdir(parents=True, exist_ok=True)
    subprocess.run([dpkg_deb, "--build", "--root-owner-group", stage_dir, str(output_path)], check=True)
    return output_path


def build_rpm(stage_dir, output_dir, version, release, arch, *,
              linking="DYNAMIC", pack_frontend=True, client_only=False,
              install_stdext=False):
    rpmbuild = require_tool("rpmbuild")
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="daffy-rpmbuild-") as topdir:
        topdir_path = pathlib.Path(topdir)
        for dirname in ["BUILD", "RPMS", "SOURCES", "SPECS", "SRPMS", "BUILDROOT", "tmp"]:
            (topdir_path / dirname).mkdir(parents=True, exist_ok=True)
        spec_path = topdir_path / "SPECS" / "daffychat.spec"
        
        # Build %post script
        post_lines = [
            "DAFFY_DATA=/usr/share/daffychat",
            "DAFFY_SCRIPTS=$DAFFY_DATA/scripts",
            "",
            "# Run DaffyChat post-install helper",
            "DAFFY_PREFIX=/usr/local \\",
            f"DAFFY_CLIENT_ONLY={'1' if client_only else '0'} \\",
            f"DAFFY_PACK_FRONTEND={'1' if pack_frontend else '0'} \\",
            f"DAFFY_INSTALL_STDEXT={'1' if install_stdext else '0'} \\",
            '  sh "$DAFFY_SCRIPTS/post-install.sh"',
        ]
        
        if linking == "PACK":
            post_lines += [
                "",
                "# Refresh shared-library cache for packed vendored libs",
                "ldconfig 2>/dev/null || true",
            ]
        
        spec_lines = [
            "Name: daffychat",
            f"Version: {version}",
            f"Release: {release}%{{?dist}}",
            "Summary: Native-first peer-to-peer voice chat bootstrap package",
            "License: Proprietary",
            "URL: https://daffychat.local",
            f"BuildArch: {arch['rpm']}",
            "",
            "%description",
            "Native-first peer-to-peer voice chat bootstrap package.",
            "",
            "%prep",
            "",
            "%build",
            "",
            "%install",
            "rm -rf %{buildroot}",
            f"cp -a {pathlib.Path(stage_dir).resolve()}/. %{{buildroot}}/",
            "",
            "%files",
            "%defattr(-,root,root,-)",
            *packaged_paths(stage_dir),
            "",
            "%post",
            *post_lines,
            "",
            "%changelog",
            f"* {time.strftime('%a %b %d %Y')} DaffyChat <maintainers@daffychat.local> - {version}-{release}",
            "- Automated local package build",
            "",
        ]
        
        spec_path.write_text("\n".join(spec_lines), encoding="utf-8")
        subprocess.run([rpmbuild, "--define", f"_topdir {topdir_path}", "--define", f"_tmppath {topdir_path / 'tmp'}", "-bb", str(spec_path)], check=True)
        rpm_candidates = list((topdir_path / "RPMS").rglob("*.rpm"))
        if not rpm_candidates:
            raise SystemExit("rpmbuild did not produce an rpm artifact")
        output_path = output_dir / rpm_candidates[0].name
        shutil.copy2(rpm_candidates[0], output_path)
        return output_path


def build_pacman(stage_dir, output_dir, version, release, arch, *,
                 linking="DYNAMIC", pack_frontend=True, client_only=False,
                 install_stdext=False):
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    builddate = int(time.time())
    pkginfo = "\n".join([
        "# Generated by DaffyChat packaging helper",
        "pkgname = daffychat",
        "pkgbase = daffychat",
        f"pkgver = {version}-{release}",
        "pkgdesc = Native-first peer-to-peer voice chat bootstrap package",
        f"builddate = {builddate}",
        "packager = DaffyChat <maintainers@daffychat.local>",
        f"size = {sum((pathlib.Path(root) / name).stat().st_size for root, _, files in os.walk(stage_dir) for name in files)}",
        f"arch = {arch['pacman']}",
        "license = custom",
    ]) + "\n"
    tar_path = output_dir / f"daffychat-{version}-{release}-{arch['pacman']}.pkg.tar"
    with tempfile.TemporaryDirectory(prefix="daffy-pacman-meta-") as meta_dir:
        meta_path = pathlib.Path(meta_dir)
        meta_path.joinpath(".PKGINFO").write_text(pkginfo, encoding="utf-8")
        
        # Create .INSTALL script for post-install actions
        install_lines = [
            "post_install() {",
            "  DAFFY_DATA=/usr/share/daffychat",
            "  DAFFY_SCRIPTS=$DAFFY_DATA/scripts",
            "",
            "  # Run DaffyChat post-install helper",
            "  DAFFY_PREFIX=/usr/local \\",
            f"  DAFFY_CLIENT_ONLY={'1' if client_only else '0'} \\",
            f"  DAFFY_PACK_FRONTEND={'1' if pack_frontend else '0'} \\",
            f"  DAFFY_INSTALL_STDEXT={'1' if install_stdext else '0'} \\",
            '    sh "$DAFFY_SCRIPTS/post-install.sh"',
        ]
        
        if linking == "PACK":
            install_lines += [
                "",
                "  # Refresh shared-library cache for packed vendored libs",
                "  ldconfig 2>/dev/null || true",
            ]
        
        install_lines.append("}")
        meta_path.joinpath(".INSTALL").write_text("\n".join(install_lines) + "\n", encoding="utf-8")
        
        with tarfile.open(tar_path, "w") as archive:
            archive.add(meta_path / ".PKGINFO", arcname=".PKGINFO")
            archive.add(meta_path / ".INSTALL", arcname=".INSTALL")
            for root, dirs, files in os.walk(stage_dir):
                dirs.sort(); files.sort()
                for filename in files:
                    path = pathlib.Path(root) / filename
                    archive.add(path, arcname=str(path.relative_to(stage_dir)))
    return tar_path


def build_tgz(stage_dir, output_dir, version, release):
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"daffychat-{version}-{release}.tar.gz"
    with tarfile.open(output_path, "w:gz") as archive:
        archive.add(stage_dir, arcname="daffychat")
    return output_path


def build_single_file_archive(stage_dir, output_dir, version, release, mode):
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    base_name = output_dir / f"daffychat-{version}-{release}.tar"
    with tarfile.open(base_name, "w") as archive:
        archive.add(stage_dir, arcname="daffychat")
    raw = base_name.read_bytes()
    if mode == "gz":
        out = pathlib.Path(str(base_name) + ".gz")
        with gzip.open(out, "wb") as fh:
            fh.write(raw)
    elif mode == "bz2":
        out = pathlib.Path(str(base_name) + ".bz2")
        out.write_bytes(bz2.compress(raw))
    elif mode == "zlib":
        out = pathlib.Path(str(base_name) + ".zz")
        out.write_bytes(zlib.compress(raw, level=9))
    else:
        raise SystemExit(f"unsupported archive mode: {mode}")
    base_name.unlink()
    return out


def main():
    args = parse_args()
    linking      = args.linking
    pack_frontend = args.pack_frontend
    client_only  = args.client_only
    install_stdext = args.with_stdext
    install_coturn = args.with_coturn
    stamp = args.stamp or time.strftime("%Y%m%d%H%M%S")
    version = args.version
    release = args.release
    arch = detect_arch()
    with tempfile.TemporaryDirectory(prefix=f"daffy-stage-{stamp}-") as stage_dir:
        install_tree(args.source_dir, args.build_dir, stage_dir,
                     linking=linking,
                     pack_frontend=pack_frontend,
                     client_only=client_only,
                     install_stdext=install_stdext,
                     install_coturn=install_coturn)
        fmt = "tgz" if args.format == "tarball" else args.format
        if fmt == "deb":
            output = build_deb(stage_dir, args.output_dir, version, release, arch,
                               linking=linking,
                               pack_frontend=pack_frontend,
                               client_only=client_only,
                               install_stdext=install_stdext)
        elif fmt == "rpm":
            output = build_rpm(stage_dir, args.output_dir, version, release, arch,
                               linking=linking,
                               pack_frontend=pack_frontend,
                               client_only=client_only,
                               install_stdext=install_stdext)
        elif fmt == "pacman":
            output = build_pacman(stage_dir, args.output_dir, version, release, arch,
                                  linking=linking,
                                  pack_frontend=pack_frontend,
                                  client_only=client_only,
                                  install_stdext=install_stdext)
        elif fmt == "tgz":
            output = build_tgz(stage_dir, args.output_dir, version, release)
        else:
            output = build_single_file_archive(stage_dir, args.output_dir, version, release, fmt)
        print(output)


if __name__ == "__main__":
    main()
