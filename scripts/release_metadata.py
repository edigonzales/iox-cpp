#!/usr/bin/env python3
"""Validate and render the committed iox native dependency lock."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys


SHA_RE = re.compile(r"^[0-9a-f]{40}$")
SHA512_RE = re.compile(r"^[0-9a-f]{128}$")


def load_lock(root: pathlib.Path) -> dict:
    path = root / "release" / "dependencies.lock.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1 or data.get("project") != "iox-cpp":
        raise ValueError("unsupported iox dependency lock")
    ilic = data.get("dependencies", {}).get("ilic", {})
    if not SHA_RE.fullmatch(ilic.get("sourceSha", "")):
        raise ValueError("ilic sourceSha must be a full lowercase Git SHA")
    if not SHA512_RE.fullmatch(ilic.get("archiveSha512", "")):
        raise ValueError("ilic archiveSha512 must be a lowercase SHA512")
    vcpkg = data.get("vcpkg", {})
    if not SHA_RE.fullmatch(vcpkg.get("builtinBaseline", "")):
        raise ValueError("vcpkg builtinBaseline must be a full Git SHA")
    if not SHA_RE.fullmatch(vcpkg.get("registry", {}).get("baseline", "")):
        raise ValueError("custom registry baseline must be a full Git SHA")
    version = ilic.get("version", "")
    if not re.fullmatch(
        r"[0-9]+\.[0-9]+\.[0-9]+(?:-snapshot\.(?:g[0-9a-f]{12}|[0-9a-f]{8}))?",
        version,
    ):
        raise ValueError("ilic version is neither a supported legacy nor current vcpkg version")
    return data


def rendered_files(data: dict) -> dict[pathlib.Path, str]:
    ilic = data["dependencies"]["ilic"]
    vcpkg = data["vcpkg"]
    manifest = {
        "name": "iox02-ci-dependencies",
        "version-string": "0",
        "dependencies": ["expat", "yyjson", "ilic"],
        "overrides": [{"name": "ilic", "version-string": ilic["version"]}],
    }
    configuration = {
        "default-registry": {
            "kind": "builtin",
            "baseline": vcpkg["builtinBaseline"],
        },
        "registries": [
            {
                "kind": "git",
                "repository": vcpkg["registry"]["repository"],
                "reference": vcpkg["registry"]["reference"],
                "baseline": vcpkg["registry"]["baseline"],
                "packages": ["ilic"],
            }
        ],
    }
    overlay_manifest = {
        "name": "iox02-vcpkg-smoke",
        "version-string": "0",
        "dependencies": [{"name": "iox-cpp", "features": ["ilic"]}],
        "overrides": [{"name": "ilic", "version-string": ilic["version"]}],
    }
    return {
        pathlib.Path("release/vcpkg-dependencies/vcpkg.json"): json.dumps(manifest, indent=2) + "\n",
        pathlib.Path("release/vcpkg-dependencies/vcpkg-configuration.json"): json.dumps(configuration, indent=2) + "\n",
        pathlib.Path("release/vcpkg-overlay-consumer/vcpkg.json"): json.dumps(overlay_manifest, indent=2) + "\n",
        pathlib.Path("release/vcpkg-overlay-consumer/vcpkg-configuration.json"): json.dumps(configuration, indent=2) + "\n",
    }


def sync(root: pathlib.Path, check_only: bool) -> None:
    expected = rendered_files(load_lock(root))
    stale = []
    for relative, content in expected.items():
        path = root / relative
        current = path.read_text(encoding="utf-8") if path.exists() else None
        if current != content:
            stale.append(str(relative))
            if not check_only:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
    if stale and check_only:
        raise ValueError("generated dependency files are stale: " + ", ".join(stale))


def export_github_env(root: pathlib.Path, output: pathlib.Path) -> None:
    data = load_lock(root)
    ilic = data["dependencies"]["ilic"]
    vcpkg = data["vcpkg"]
    lines = {
        "IOX_VCPKG_REF": vcpkg["toolRef"],
        "IOX_VCPKG_BUILTIN_BASELINE": vcpkg["builtinBaseline"],
        "IOX_REGISTRY_BASELINE": vcpkg["registry"]["baseline"],
        "IOX_ILIC_PACKAGE_VERSION": ilic["version"],
        "IOX_ILIC_SOURCE_SHA": ilic["sourceSha"],
    }
    with output.open("a", encoding="utf-8") as stream:
        for key, value in lines.items():
            stream.write(f"{key}={value}\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", type=pathlib.Path, default=pathlib.Path.cwd())
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("check")
    sub.add_parser("sync")
    export = sub.add_parser("export-github-env")
    export.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    root = args.project_root.resolve()
    try:
        if args.command == "check":
            sync(root, True)
            print("dependency lock is consistent")
        elif args.command == "sync":
            sync(root, False)
            print("dependency files synchronized")
        else:
            export_github_env(root, args.output)
    except ValueError as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
