# iox-cpp vcpkg support

The native SDK is available as the `iox-cpp` vcpkg port. The CMake package is
named `iox` and exports targets under the `iox::` namespace.

The checked-in `ports/iox-cpp` directory is an overlay/template at the last
locally verified source revision, not the version catalogue. Published
immutable versions live in the shared `ilic-fork/vcpkg-registry` branch and
are created from this template by the centralized registry workflow.

The port is additive. Existing `add_subdirectory()` and FetchContent users
keep the source-build default. Installed consumers use:

```cmake
find_package(iox CONFIG REQUIRED)
target_link_libraries(app PRIVATE iox::xtf iox::core)
```

Use the `ilic` feature for model-aware XTF support:

```json
{"name":"iox-cpp","features":["ilic"]}
```

The optional `geos` feature enables GEOS-backed geometry validation. The
published package is static and currently supports `x64-linux`, `arm64-osx`,
`x64-windows`, and `x64-windows-static`.

## Local overlay

With a vcpkg checkout in `$VCPKG_ROOT`:

```sh
$VCPKG_ROOT/vcpkg install iox-cpp[ilic]:x64-linux \
  --overlay-ports="$PWD/vcpkg/ports"
```

For the current checkout, `vcpkg-overlay-02.yml` and
`scripts/prepare-vcpkg-registry-port.mjs` replace the source SHA, archive
SHA512, and version before building.

## Binary cache

The vcpkg workflows publish the four supported triplets to the
`nuget.pkg.github.com/edigonzales` feed and verify clean restoration with
`--only-binarycaching`. Package consumers must grant their GitHub Actions
workflow read access to the package or use a credential with `read:packages`.
