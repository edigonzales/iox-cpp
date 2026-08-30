# Build- und Publikationspipeline

`iox-cpp` wird im kanonischen GitHub-Repository entwickelt. CI läuft auf
`main`, Pull Requests und Release-Tags. Dabei wird der aktuelle iox-Quellcode
immer selbst gebaut; bereits veröffentlichte native Abhängigkeiten werden in
der reproduzierbaren CI bevorzugt aus dem vcpkg-Binary-Cache restauriert.

## Workflows

| Workflow | Trigger | Rolle |
| --- | --- | --- |
| `ci.yml` | `main`, Pull Requests, `v*`, manuell | blockierende native/WASM-Prüfung |
| `ilic-main-canary.yml` | geplant oder manuell | nicht blockierender Test gegen aktuelles `ilic/main` |
| `publish-iox.yml` | manuell für Snapshot, neuer `vX.Y.Z`-Tag für Stable | Source-/WASM-Artefakte und Publikationsanfrage |
| `vcpkg-version-publish.yml` | interne Anfrage oder manuell | Anfrage an den zentralen Registry-Schreiber und validiertes Warten |
| `vcpkg-binary-cache.yml` | validierte Registry-Version | Binary-Pakete plus strikter Restore |

Ein grüner `main`-Build publiziert nichts automatisch. Snapshots werden
koordiniert über `workflow_dispatch` gestartet und verwenden in npm und vcpkg
dieselbe Identität:

```text
0.2.0-snapshot.g<12 Zeichen des iox-Git-SHA>
```

Datum und GitHub-Run-ID stehen in `interlis-release.json`, nicht in der
Versionsnummer. Das Manifest enthält den vollständigen iox-SHA, die exakte
ilic-/Expat-/yyjson-Kombination und die vcpkg-Baselines. Der npm-Tarball enthält
zusätzlich `gitHead`. Der Dist-Tag `snapshot` zeigt nur auf Vorabversionen;
`latest` wird ausschließlich durch ein stabiles `vX.Y.Z`-Release gesetzt.

## Native Abhängigkeiten und Binary Cache

Die Rollen sind bewusst getrennt:

- CI baut iox aus dem ausgecheckten Quellcode;
- die gelockten ilic-, Expat- und yyjson-Pakete kommen intern strikt aus dem
  Binary Cache;
- externe Forks ohne Paketberechtigung dürfen diese Abhängigkeiten aus Source
  bauen;
- der Binary-Publisher baut `ilic`, `geos` und `ilic,geos` für Linux, macOS,
  Windows und Windows-static und prüft danach jede Variante mit
  `--only-binarycaching`;
- eine featurelose iox-Variante wird mangels Consumer nicht publiziert.

Die gemeinsame Registry liegt auf `ilic-fork/vcpkg-registry`. Nur der dortige
serialisierte Workflow darf sie schreiben. `iox-cpp` sendet mit dem bestehenden
`VCPKG_REGISTRY_TOKEN` lediglich einen `repository_dispatch`, wartet auf die
öffentlich sichtbare, inhaltlich passende Version und startet danach mit dem
normalen `GITHUB_TOKEN` den eigenen Binary-Publisher. Das Secret benötigt daher
nur Dispatch-Rechte auf `ilic-fork`, keinen Branch-Schreibzugriff.

## Source- und Paketverträge

Der stabile Source-Fetch-Vertrag bleibt ilic `v0.9.10`. Dieser alte Stand wird
direkt aus Source eingebunden. Die reproduzierbare vcpkg-CI verwendet dagegen
den exakten Snapshot aus `release/dependencies.lock.json`; aktuelles
`ilic/main` läuft nur im Canary. Dadurch werden stabile Kompatibilität,
reproduzierbare Paketkombination und frühe Warnung vor zukünftigen Änderungen
nicht miteinander vermischt.

`@interlis/iox-wasm` ist derzeit noch nicht auf npm publiziert. Vor dem ersten
Publish muss die Paket-/Owner-/2FA-Einrichtung einmalig abgeschlossen und der
Trusted Publisher auf `edigonzales/iox-cpp` sowie
`.github/workflows/publish-iox.yml` eingeschränkt werden. Bis dieser Bootstrap
erfolgreich war, darf die Dokumentation das Paket nicht als verfügbar
bezeichnen.
