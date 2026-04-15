#!/usr/bin/env python3
import argparse
import os
import pathlib
import shutil
import subprocess
import tarfile
import tempfile
import time


BINARIES = [
    ("daffy-backend", "usr/bin/daffy-backend"),
    ("daffy-signaling", "usr/bin/daffy-signaling"),
    ("dssl-bindgen", "usr/bin/dssl-bindgen"),
    ("dssl-docstrip", "usr/bin/dssl-docstrip"),
    ("dssl-docgen", "usr/bin/dssl-docgen"),
    ("daffyc", "usr/bin/daffyc"),
    ("libdaffy_core.a", "usr/lib/libdaffy_core.a"),
]

DATA_FILES = [
    ("config/daffychat.example.json", "usr/share/daffychat/config/daffychat.example.json"),
    ("frontend/index.html", "usr/share/daffychat/frontend/index.html"),
    ("frontend/room.html", "usr/share/daffychat/frontend/room.html"),
    ("frontend/guide.html", "usr/share/daffychat/frontend/guide.html"),
    ("frontend/403.html", "usr/share/daffychat/frontend/403.html"),
    ("frontend/404.html", "usr/share/daffychat/frontend/404.html"),
    ("frontend/bridge.js", "usr/share/daffychat/frontend/bridge.js"),
    ("frontend/app/styles/daffy.css", "usr/share/daffychat/frontend/app/styles/daffy.css"),
    ("frontend/app/state/store.js", "usr/share/daffychat/frontend/app/state/store.js"),
    ("frontend/app/api/signaling.js", "usr/share/daffychat/frontend/app/api/signaling.js"),
    ("frontend/app/hooks/bridge-hooks.js", "usr/share/daffychat/frontend/app/hooks/bridge-hooks.js"),
    (
        "frontend/app/components/message-renderer.js",
        "usr/share/daffychat/frontend/app/components/message-renderer.js",
    ),
    (
        "frontend/app/components/theme-toggle.js",
        "usr/share/daffychat/frontend/app/components/theme-toggle.js",
    ),
]

DATA_DIRS = [
    ("stdext", "usr/share/daffychat/stdext"),
]


def parse_args():
    parser = argparse.ArgumentParser(description="Build DaffyChat package artifacts from an existing build directory.")
    parser.add_argument("--format", choices=["deb", "rpm", "pacman"], required=True)
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--release", default="1")
    parser.add_argument("--stamp")
    return parser.parse_args()


def require_tool(name):
    tool = shutil.which(name)
    if not tool:
        raise SystemExit(f"required packaging tool not found: {name}")
    return tool


def detect_arch():
    machine = os.uname().machine
    deb_map = {
        "x86_64": "amd64",
        "aarch64": "arm64",
    }
    return {
        "native": machine,
        "deb": deb_map.get(machine, machine),
        "rpm": machine,
        "pacman": machine,
    }


def install_tree(source_dir, build_dir, stage_dir):
    for source_name, destination in BINARIES:
        source_path = pathlib.Path(build_dir) / source_name
        if not source_path.exists():
            raise SystemExit(f"expected build output is missing: {source_path}")
        destination_path = pathlib.Path(stage_dir) / destination
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, destination_path)

    for source_name, destination in DATA_FILES:
        source_path = pathlib.Path(source_dir) / source_name
        if not source_path.exists():
            raise SystemExit(f"expected source asset is missing: {source_path}")
        destination_path = pathlib.Path(stage_dir) / destination
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, destination_path)

    for source_name, destination in DATA_DIRS:
        source_path = pathlib.Path(source_dir) / source_name
        if not source_path.exists():
            raise SystemExit(f"expected source directory is missing: {source_path}")
        destination_path = pathlib.Path(stage_dir) / destination
        if destination_path.exists():
            shutil.rmtree(destination_path)
        shutil.copytree(source_path, destination_path)


def packaged_paths(stage_dir):
    paths = []
    for root, dirs, files in os.walk(stage_dir):
        dirs.sort()
        files.sort()
        root_path = pathlib.Path(root)
        for filename in files:
            path = root_path / filename
            relpath = "/" + str(path.relative_to(stage_dir))
            paths.append(relpath)
    return paths


def build_deb(stage_dir, output_dir, version, release, arch):
    dpkg_deb = require_tool("dpkg-deb")
    control_dir = pathlib.Path(stage_dir) / "DEBIAN"
    control_dir.mkdir(parents=True, exist_ok=True)
    installed_size_kb = 0
    for root, _, files in os.walk(stage_dir):
        for filename in files:
            path = pathlib.Path(root) / filename
            if "DEBIAN" not in path.parts:
                installed_size_kb += (path.stat().st_size + 1023) // 1024
    control_dir.joinpath("control").write_text(
        "\n".join(
            [
                "Package: daffychat",
                f"Version: {version}-{release}",
                "Section: net",
                "Priority: optional",
                f"Architecture: {arch['deb']}",
                "Maintainer: DaffyChat <maintainers@daffychat.local>",
                f"Installed-Size: {max(installed_size_kb, 1)}",
                "Description: Native-first peer-to-peer voice chat bootstrap package",
                "",
            ]
        ),
        encoding="utf-8",
    )
    output_path = pathlib.Path(output_dir) / f"daffychat_{version}-{release}_{arch['deb']}.deb"
    subprocess.run([dpkg_deb, "--build", "--root-owner-group", stage_dir, str(output_path)], check=True)
    return output_path


def build_rpm(stage_dir, output_dir, version, release, arch):
    rpmbuild = require_tool("rpmbuild")
    output_dir = pathlib.Path(output_dir)
    with tempfile.TemporaryDirectory(prefix="daffy-rpmbuild-") as topdir:
        topdir_path = pathlib.Path(topdir)
        for dirname in ["BUILD", "RPMS", "SOURCES", "SPECS", "SRPMS", "BUILDROOT", "tmp"]:
            (topdir_path / dirname).mkdir(parents=True, exist_ok=True)

        spec_path = topdir_path / "SPECS" / "daffychat.spec"
        file_lines = packaged_paths(stage_dir)
        spec_path.write_text(
            "\n".join(
                [
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
                    *file_lines,
                    "",
                    "%changelog",
                    f"* {time.strftime('%a %b %d %Y')} DaffyChat <maintainers@daffychat.local> - {version}-{release}",
                    "- Automated local package build",
                    "",
                ]
            ),
            encoding="utf-8",
        )

        subprocess.run(
            [
                rpmbuild,
                "--define",
                f"_topdir {topdir_path}",
                "--define",
                f"_tmppath {topdir_path / 'tmp'}",
                "-bb",
                str(spec_path),
            ],
            check=True,
        )

        rpm_candidates = list((topdir_path / "RPMS").rglob("*.rpm"))
        if not rpm_candidates:
            raise SystemExit("rpmbuild did not produce an rpm artifact")
        output_path = output_dir / rpm_candidates[0].name
        shutil.copy2(rpm_candidates[0], output_path)
        return output_path


def build_pacman(stage_dir, output_dir, version, release, arch):
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    builddate = int(time.time())
    pkginfo = "\n".join(
        [
            "# Generated by DaffyChat packaging helper",
            "pkgname = daffychat",
            f"pkgbase = daffychat",
            f"pkgver = {version}-{release}",
            "pkgdesc = Native-first peer-to-peer voice chat bootstrap package",
            f"builddate = {builddate}",
            f"packager = DaffyChat <maintainers@daffychat.local>",
            f"size = {sum((pathlib.Path(root) / name).stat().st_size for root, _, files in os.walk(stage_dir) for name in files)}",
            f"arch = {arch['pacman']}",
            "license = custom",
        ]
    ) + "\n"

    tar_path = output_dir / f"daffychat-{version}-{release}-{arch['pacman']}.pkg.tar"
    with tempfile.TemporaryDirectory(prefix="daffy-pacman-meta-") as meta_dir:
        meta_path = pathlib.Path(meta_dir)
        meta_path.joinpath(".PKGINFO").write_text(pkginfo, encoding="utf-8")
        with tarfile.open(tar_path, "w") as archive:
            archive.add(meta_path / ".PKGINFO", arcname=".PKGINFO")
            for root, dirs, files in os.walk(stage_dir):
                dirs.sort()
                files.sort()
                for filename in files:
                    path = pathlib.Path(root) / filename
                    archive.add(path, arcname=str(path.relative_to(stage_dir)))

    zstd = shutil.which("zstd")
    if zstd:
        output_path = pathlib.Path(str(tar_path) + ".zst")
        subprocess.run([zstd, "-q", "-f", "-o", str(output_path), str(tar_path)], check=True)
        tar_path.unlink()
        return output_path
    return tar_path


def main():
    args = parse_args()
    source_dir = pathlib.Path(args.source_dir).resolve()
    build_dir = pathlib.Path(args.build_dir).resolve()
    output_dir = pathlib.Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    arch = detect_arch()
    with tempfile.TemporaryDirectory(prefix=f"daffy-{args.format}-stage-") as stage_dir:
        install_tree(source_dir, build_dir, stage_dir)
        if args.format == "deb":
            artifact = build_deb(stage_dir, output_dir, args.version, args.release, arch)
        elif args.format == "rpm":
            artifact = build_rpm(stage_dir, output_dir, args.version, args.release, arch)
        else:
            artifact = build_pacman(stage_dir, output_dir, args.version, args.release, arch)

    if args.stamp:
        pathlib.Path(args.stamp).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.stamp).write_text(str(artifact) + "\n", encoding="utf-8")

    print(artifact)


if __name__ == "__main__":
    main()
