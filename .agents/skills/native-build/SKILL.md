---
name: native-build
description: Configure, build, test, sanitize, and inspect native iox-cpp targets on macOS, Linux, and Windows using the repository's CMake scripts.
---

# Native build skill

## Standard flow

```sh
./scripts/build-native.sh
./scripts/test-native.sh
```

Use separate directories under `build/`; never build in the source tree.

## Requirements

- CMake 3.20+
- C++17 without compiler extensions
- CTest registration for every test executable
- project warnings enabled and optionally treated as errors
- static project libraries
- pinned dependencies through `cmake/IoxDependencies.cmake`

## Diagnosis order

1. Reproduce with the standard script.
2. Inspect `CMakeCache.txt` for stale compiler/toolchain values.
3. Reconfigure in a clean build directory.
4. Build the smallest failing target verbosely.
5. Fix code or CMake; do not weaken warnings globally.
6. Re-run the full native test suite.

## Platform constraints

- macOS target is ARM64.
- Linux target is x86_64 with GCC or Clang.
- Windows target is x86_64 with MSVC.
- Public headers must compile from C and C++ smoke consumers where applicable.

Do not add CI workflows. Local reproducible commands are the deliverable.
