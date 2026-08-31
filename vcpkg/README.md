# vcpkg-Port von iox-cpp

Der Port exportiert das CMake-Paket `iox` mit Targets im Namespace `iox::`.
`ilic` aktiviert die modellbewusste Integration, `geos` die GEOS-gestützte
Geometrieprüfung.

```sh
$VCPKG_ROOT/vcpkg install iox-cpp[ilic]:x64-linux \
  --overlay-ports="$PWD/vcpkg/ports"
```

Die lokale Portdatei ist eine Entwicklungsvorlage. Publizierte Versionen
liegen im gemeinsamen Registry-Branch von `ilic-fork`. Versionierung,
Binary-Cache und alle unterstützten Varianten stehen unter
[`docs/release.md`](../docs/release.md#vcpkg-und-binary-cache).
