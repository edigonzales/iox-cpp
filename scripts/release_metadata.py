#!/usr/bin/env python3
"""Validate iox dependencies and create deterministic release metadata."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import pathlib
import re
import subprocess
import sys


SEMVER_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
SHA512_RE = re.compile(r"^[0-9a-f]{128}$")
SNAPSHOT_RE = re.compile(
    r"^(?P<base>[0-9]+\.[0-9]+\.[0-9]+)-snapshot\.g(?P<sha>[0-9a-f]{12})$"
)
RUNTIME_VERSION_RE = re.compile(
    r"^[0-9]+\.[0-9]+\.[0-9]+(?:-SNAPSHOT|-snapshot\.g[0-9a-f]{12})?$"
)


def require_semver(value: str) -> str:
    if not SEMVER_RE.fullmatch(value):
        raise ValueError(f"version must be X.Y.Z, got {value!r}")
    return value


def require_sha(value: str) -> str:
    if not SHA_RE.fullmatch(value):
        raise ValueError("source SHA must be a lowercase 40-character Git SHA")
    return value


def snapshot_version(base_version: str, source_sha: str) -> str:
    return f"{require_semver(base_version)}-snapshot.g{require_sha(source_sha)[:12]}"


def validate_artifact_version(version: str, source_sha: str) -> str:
    require_sha(source_sha)
    if SEMVER_RE.fullmatch(version):
        return "stable"
    match = SNAPSHOT_RE.fullmatch(version)
    if not match:
        raise ValueError(
            "artifact version must be X.Y.Z or X.Y.Z-snapshot.g<12-character-source-sha>"
        )
    if match.group("sha") != source_sha[:12]:
        raise ValueError("snapshot suffix does not match the source SHA")
    return "snapshot"


def project_version(root: pathlib.Path) -> str:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    matches = re.findall(
        r"project\s*\(\s*iox-cpp\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)(?=\s|\))",
        cmake,
        flags=re.IGNORECASE,
    )
    if len(matches) != 1:
        raise ValueError(f"expected one project(iox-cpp VERSION X.Y.Z), found {len(matches)}")
    version = require_semver(matches[0])
    package = json.loads(
        (root / "packages/iox-wasm/package.json").read_text(encoding="utf-8")
    )
    if package.get("version") != version:
        raise ValueError(
            f"@interlis/iox-wasm version {package.get('version')} does not match {version}"
        )
    return version


def git_sha(root: pathlib.Path) -> str:
    value = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True
    ).strip()
    return require_sha(value)


def load_lock(root: pathlib.Path) -> dict:
    path = root / "release" / "dependencies.lock.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1 or data.get("project") != "iox-cpp":
        raise ValueError("unsupported iox dependency lock")
    ilic = data.get("dependencies", {}).get("ilic", {})
    require_sha(ilic.get("sourceSha", ""))
    if not SHA512_RE.fullmatch(ilic.get("archiveSha512", "")):
        raise ValueError("ilic archiveSha512 must be a lowercase SHA512")
    vcpkg = data.get("vcpkg", {})
    require_sha(vcpkg.get("builtinBaseline", ""))
    require_sha(vcpkg.get("registry", {}).get("baseline", ""))
    version = ilic.get("version", "")
    if not re.fullmatch(
        r"[0-9]+\.[0-9]+\.[0-9]+(?:-snapshot\.(?:g[0-9a-f]{12}|[0-9a-f]{8}))?",
        version,
    ):
        raise ValueError("ilic version is neither a supported legacy nor current vcpkg version")
    runtime_version = ilic.get("runtimeVersion", "")
    if not RUNTIME_VERSION_RE.fullmatch(runtime_version):
        raise ValueError("ilic runtimeVersion is neither stable nor a supported snapshot")
    if runtime_version.split("-", 1)[0] != version.split("-", 1)[0]:
        raise ValueError("ilic runtimeVersion and vcpkg version must share a base version")
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


def check(root: pathlib.Path) -> None:
    project_version(root)
    sync(root, True)


def release_manifest(
    *,
    root: pathlib.Path,
    artifact_version: str,
    source_sha: str,
    run_id: str | None,
    published_at: str | None,
    toolchain: str | None,
) -> dict[str, object]:
    kind = validate_artifact_version(artifact_version, source_sha)
    if artifact_version.split("-", 1)[0] != project_version(root):
        raise ValueError("artifact version does not match the project base version")
    if published_at:
        parsed = dt.datetime.fromisoformat(published_at.replace("Z", "+00:00"))
        if parsed.tzinfo is None:
            raise ValueError("published-at must include a timezone")
    lock = load_lock(root)
    return {
        "schemaVersion": 1,
        "project": "iox-cpp",
        "artifactVersion": artifact_version,
        "versionKind": kind,
        "sourceSha": source_sha,
        "dependencies": lock["dependencies"],
        "vcpkg": lock["vcpkg"],
        "build": {
            "githubRunId": run_id or None,
            "publishedAt": published_at or None,
            "toolchain": toolchain or None,
        },
    }


def export_github_env(root: pathlib.Path, output: pathlib.Path) -> None:
    data = load_lock(root)
    ilic = data["dependencies"]["ilic"]
    vcpkg = data["vcpkg"]
    lines = {
        "IOX_VCPKG_REF": vcpkg["toolRef"],
        "IOX_VCPKG_BUILTIN_BASELINE": vcpkg["builtinBaseline"],
        "IOX_REGISTRY_BASELINE": vcpkg["registry"]["baseline"],
        "IOX_ILIC_PACKAGE_VERSION": ilic["version"],
        "IOX_ILIC_RUNTIME_VERSION": ilic["runtimeVersion"],
        "IOX_ILIC_SOURCE_SHA": ilic["sourceSha"],
        "IOX_ILIC_ARCHIVE_SHA512": ilic["archiveSha512"],
    }
    with output.open("a", encoding="utf-8") as stream:
        for key, value in lines.items():
            stream.write(f"{key}={value}\n")


def github_env_output(output: pathlib.Path | None) -> pathlib.Path:
    if output is not None:
        return output
    value = os.environ.get("GITHUB_ENV")
    if not value:
        raise ValueError("export-github-env requires --output or GITHUB_ENV")
    return pathlib.Path(value)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    result.add_argument("--project-root", type=pathlib.Path, default=pathlib.Path.cwd())
    sub = result.add_subparsers(dest="command", required=True)
    sub.add_parser("check")
    sub.add_parser("sync")
    version = sub.add_parser("version")
    version.add_argument("--source-sha")
    version.add_argument("--kind", choices=("snapshot", "stable"), default="snapshot")
    version.add_argument("--tag")
    manifest = sub.add_parser("manifest")
    manifest.add_argument("--artifact-version", required=True)
    manifest.add_argument("--source-sha", required=True)
    manifest.add_argument("--run-id")
    manifest.add_argument("--published-at")
    manifest.add_argument("--toolchain")
    manifest.add_argument("--output", type=pathlib.Path, required=True)
    export = sub.add_parser("export-github-env")
    export.add_argument("--output", type=pathlib.Path)
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    root = args.project_root.resolve()
    try:
        if args.command == "check":
            check(root)
            print("release metadata is consistent")
        elif args.command == "sync":
            sync(root, False)
            print("dependency files synchronized")
        elif args.command == "version":
            base = project_version(root)
            sha = require_sha(args.source_sha or git_sha(root))
            if args.kind == "stable":
                expected_tag = f"v{base}"
                if args.tag != expected_tag:
                    raise ValueError(f"stable release requires tag {expected_tag}")
                print(base)
            else:
                print(snapshot_version(base, sha))
        elif args.command == "manifest":
            data = release_manifest(
                root=root,
                artifact_version=args.artifact_version,
                source_sha=require_sha(args.source_sha),
                run_id=args.run_id,
                published_at=args.published_at,
                toolchain=args.toolchain,
            )
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        else:
            export_github_env(root, github_env_output(args.output))
    except (ValueError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
