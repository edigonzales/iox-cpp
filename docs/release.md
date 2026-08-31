# Versionierung und Release

Die Basisversion steht in `project(iox-cpp VERSION …)` und muss mit
`packages/iox-wasm/package.json` übereinstimmen. Neue Artefakte verwenden:

```text
stabil:    X.Y.Z        Tag vX.Y.Z
Snapshot:  X.Y.Z-snapshot.g<erste 12 Zeichen des iox-Source-SHA>
```

`release/dependencies.lock.json` fixiert ilic, Expat, yyjson sowie vcpkg-
Toolstand und Registry-Baseline. `scripts/release_metadata.py` prüft den Lock,
synchronisiert daraus erzeugte vcpkg-Manifeste und erstellt
`interlis-release.json`. Bestehende ältere Snapshot-Formate bleiben
unveränderlich, werden aber nicht mehr erzeugt.

## ilic übernehmen

Eine neue ilic-Version wird nicht automatisch übernommen. Version,
Runtime-Version, vollständiger Source-SHA, Archiv-SHA512 und Registry-Baseline
werden gemeinsam in der Lock-Datei aktualisiert. Danach ausführen:

```sh
python3 scripts/release_metadata.py sync
python3 scripts/release_metadata.py check
python3 test/release_metadata_test.py
./scripts/build-native.sh
./scripts/test-native.sh
```

Der geplante Canary gegen `ilic/main` prüft nur Vorwärtskompatibilität und
ändert den Release-Lock nicht.

## Snapshot

1. Einen grünen Commit auf `main` auswählen.
2. **Publish iox-cpp** manuell auf `main` starten.
3. Der Workflow erzeugt die SHA-basierte Version, baut Native und WASM,
   publiziert `@ilic/iox-wasm` unter npm-`snapshot`, aktualisiert den
   beweglichen GitHub-Snapshot und fordert die gleichnamige vcpkg-Version an.
4. npm-`gitHead`, Provenienzmanifest, GitHub-Artefakte, Registry-Baseline und
   Binary-Cache prüfen.

Ein grüner normaler CI-Lauf publiziert nichts.

## Stabiler Release

1. Basisversion und Changelog vorbereiten; für einen stabilen Release stabile
   Abhängigkeiten im Lock verwenden.
2. Alle lokalen Gates und CI auf dem Release-Commit ausführen.
3. Das Commit als exakt passendes `vX.Y.Z` taggen und pushen.
4. **Publish iox-cpp** publiziert Source-/WASM-Artefakte, npm-`latest`, den
   GitHub Release und die vcpkg-Anforderung.
5. Die Ergebnisse prüfen und erst danach den Lock in `duckdb-interlis`
   aktualisieren.

## vcpkg und Binary-Cache

`iox-cpp` sendet eine validierte Anfrage an den zentralen Registry-Schreiber
in `ilic-fork`; nur dieser schreibt den Branch `vcpkg-registry`. Danach baut
der iox-Workflow die Features `ilic`, `geos` und `ilic+geos` für
`x64-linux`, `arm64-osx`, `x64-windows` und `x64-windows-static` und prüft
jede Variante mit `--only-binarycaching`. Eine featurelose Variante wird
nicht publiziert.

Der Cache liegt unter
`https://nuget.pkg.github.com/edigonzales/index.json`. Die vollständige
Registry-/Cache-/Consumer-Matrix steht in der
[zentralen Ökosystemdokumentation](https://github.com/edigonzales/ilic-fork/blob/main/docs/ecosystem.md#vcpkg-registry-und-binary-cache).

## npm und Fehlerbehandlung

`@ilic/iox-wasm` verwendet den auf `publish-iox.yml` beschränkten npm Trusted
Publisher. `snapshot` wird nur von Snapshots, `latest` nur von stabilen Tags
bewegt. Ein bereits vorhandenes Artefakt gilt nur dann als idempotent, wenn
vollständiger `gitHead` und Tarball-SHA512 übereinstimmen.

Teilpublikationen werden nie gelöscht oder überschrieben. Ursache beheben und
denselben unveränderten Commit erneut ausführen; er ergänzt nur fehlende
Artefakte. Erfordert die Korrektur Quelländerungen, ist eine neue Version
nötig.
