# iox-cpp

`iox-cpp` ist ein modellfreier INTERLIS-XTF-2.3/2.4-Reader und -Writer für
C++17 und WebAssembly. Die verlustfreie Kernrepräsentation ist ein geordneter
Event-Stream für Transfers, Körbe und Objekte. Optional verbindet `iox-ilic`
die Transferdaten direkt mit dem ilic-Metamodell.

## Schnellstart

```sh
./scripts/build-native.sh
./scripts/test-native.sh
```

Für WASM muss zuerst die gepinnte Emscripten-Umgebung aktiviert werden:

```sh
source /path/to/emsdk/emsdk_env.sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

Alle Buildvarianten und die lokale ilic-Integration stehen unter
[Entwicklung](docs/entwicklung.md).

## Schnittstellen

- Native Targets: `iox::core`, `iox::xtf`, optional `iox::ilic`
- C99-ABI für fremde Laufzeiten
- npm-Paket `@ilic/iox-wasm` für Node, Browser und Worker
- Diagnosewerkzeug `iox-dump`
- vcpkg-Port `iox-cpp` mit optionalen Features `ilic` und `geos`

ITF, INTERLIS 1, GML-/CSV-Konvertierung, GUI-Code und dynamische Plugins
gehören nicht zum Projektumfang.

## Dokumentation

- [Architektur](docs/architecture.md)
- [Konformität und Referenzen](docs/conformance.md)
- [WASM-API](docs/wasm.md)
- [C-ABI](docs/c-abi.md)
- [Modelldeskriptoren](docs/model-descriptors.md)
- [Release, npm und vcpkg](docs/release.md)

Die repositoryübergreifenden Abhängigkeiten und Binary-Cache-Matrix sind
zentral in der
[ilic-Ökosystemübersicht](https://github.com/edigonzales/ilic-fork/blob/main/docs/ecosystem.md)
dokumentiert.

## Lizenz

MIT, siehe [LICENSE](LICENSE).
