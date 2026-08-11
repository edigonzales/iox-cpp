# iox-cpp with vcpkg

`iox-cpp` provides an overlay port in `vcpkg/ports/iox-cpp` and publishes immutable versions to the `vcpkg-registry` branch.

## Versions

Snapshot versions use the source commit in the version identity:

```text
0.1.0-snapshot.<8-character-source-sha>
```

A stable Git tag such as `v0.1.0` publishes vcpkg version `0.1.0`. Published registry versions are immutable: rerunning publication verifies the existing port tree instead of replacing it.

The optional `ilic` feature enables the model-aware integration:

```text
iox-cpp[ilic]
```

## Registries

A consumer of `iox-cpp[ilic]` needs both the iox-cpp and ilic Git registries in `vcpkg-configuration.json`, in addition to the pinned built-in registry. Publication records exact registry commits so that the iox-cpp binary build resolves a deterministic ilic version.

Example shape:

```json
{
  "default-registry": {
    "kind": "builtin",
    "baseline": "<pinned-vcpkg-baseline>"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/edigonzales/iox-cpp.git",
      "reference": "vcpkg-registry",
      "baseline": "<iox-cpp-registry-commit>",
      "packages": ["iox-cpp"]
    },
    {
      "kind": "git",
      "repository": "https://github.com/edigonzales/ilic-fork.git",
      "reference": "vcpkg-registry",
      "baseline": "<ilic-registry-commit>",
      "packages": ["ilic"]
    }
  ]
}
```

## Binary cache

Native packages are published to the GitHub Packages NuGet feed for:

- `x64-linux`
- `arm64-osx`
- `x64-windows`

The publication workflow builds `iox-cpp[ilic]`. A separate fresh x64-linux verification uses `--only-binarycaching`; therefore the verification fails rather than silently compiling a missing package from source.

## Automation

A successful `Native CI` push run on the default `codex-port` branch requests snapshot publication. Stable `vX.Y.Z` tags are accepted only when the exact tagged commit already has a successful `Native CI` push run and the tag version matches both the CMake project version and runtime version.

The registry publisher writes only to `vcpkg-registry`. It then dispatches binary-cache publication with the immutable iox-cpp version, the exact iox-cpp registry commit, and the exact ilic registry commit used for dependency resolution.
