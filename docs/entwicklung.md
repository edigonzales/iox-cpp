# Entwicklung

## Native Standardprüfung

```sh
./scripts/build-native.sh
./scripts/test-native.sh
```

Der lokale Source-Build verwendet die in CMake gepinnten Quellen. Kein Build
benötigt Java. Die reproduzierbare GitHub-CI baut den aktuellen iox-Checkout,
stellt aber die in `release/dependencies.lock.json` festgehaltenen ilic-,
Expat- und yyjson-Pakete strikt aus dem privaten vcpkg-Binary-Cache wieder her.
Externe Forks ohne Paketberechtigung dürfen diese Abhängigkeiten aus Source
bauen.

## Gemeinsame Entwicklung mit ilic

```sh
cmake -S . -B build/ilic \
  -DBUILD_TESTING=ON -DIOX_ENABLE_ILIC=ON \
  -DIOX_ILIC_SOURCE_DIR=/path/to/ilic-fork
cmake --build build/ilic --parallel
ctest --test-dir build/ilic --output-on-failure
```

Der reproduzierbare Source-Fetch-Pfad verwendet stattdessen
`-DIOX_FETCH_ILIC=ON` und den unveränderlichen `IOX_ILIC_GIT_TAG`. In beiden
Fällen sind ilic-CLI und ilic-eigene Tests deaktiviert; der Test
`iox.test.ilic.version` kontrolliert die erwartete Runtime-Version.

## WebAssembly

```sh
source /path/to/emsdk/emsdk_env.sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

Die Emscripten-Version ist in den Buildskripten festgelegt. Browser-, Worker-
und Paketgrenzen sind unter [WASM](wasm.md) dokumentiert.

## Zusätzliche Gates

- `scripts/coverage.sh`: Coverage-Grenzen
- `scripts/run-sanitizers.sh`: ASan/UBSan
- `scripts/run-fuzz.sh`: Fuzz-Smoke
- `scripts/verify-porting-matrix.sh`: iox-ili-Konformitätsmatrix
- `python3 test/release_metadata_test.py`: Release-Vertrag

Die CI-Workflows sind die Quelle für Plattformmatrix und konkrete
Produktions-Toolchains.
