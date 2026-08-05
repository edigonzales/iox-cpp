# Build- und Publikationspipeline

`iox-cpp` wird auf Codeberg entwickelt und nach GitHub gespiegelt. Die GitHub
Actions laufen deshalb erst, wenn der gewünschte Commit auf dem GitHub-Mirror
unter `main` angekommen ist. Native Builds sind CI-Gates; veröffentlicht werden
Source und WASM.

## Workflows

| Workflow | Trigger | Ergebnis |
| --- | --- | --- |
| `.github/workflows/ci.yml` | `main`, Pull Requests, `v*`-Tags, manuell | Native Linux/macOS/Windows-Tests und WASM-Paketprüfung |
| `.github/workflows/publish-iox.yml` | erfolgreicher `CI`-Run oder manuell | npm-Snapshot bzw. stabile npm-/GitHub-Release |

Snapshots verwenden:

```text
0.2.0-SNAPSHOT.YYYYMMDDHHmmss.<run-id>
```

Sie werden mit dem npm-Dist-Tag `snapshot` und in der beweglichen GitHub-
Pre-Release `snapshot` veröffentlicht. Stable-Releases verwenden `v0.2.0`,
npm `latest` und eine unveränderliche GitHub-Release.

Jedes Release-Manifest enthält den iox-Commit, die verwendete `ilic`-Version,
den exakten `ilic`-Commit, den Kanal und die Workflow-Run-ID. Snapshot-Builds
lösen `ilic-fork/main` genau einmal zu einem vollständigen SHA auf. Bei einem
getaggten iox-Release wird der in `cmake/IoxDependencies.cmake` deklarierte
ilic-Tag aufgelöst; ein Floating Branch ist nicht zulässig.

## Veröffentlichte Artefakte

Die GitHub-Release enthält:

- `@interlis/iox-wasm` als npm-Tarball;
- ein Source-Archiv;
- `SHA256SUMS` und `release-manifest.json`.

Native Bibliotheken und Programme werden auf allen CI-Plattformen getestet,
aber nicht als SDK- oder Binary-Release verteilt.

## Trusted Publisher und Mirror-Prüfung

Der npm Trusted Publisher für `@interlis/iox-wasm` muss auf
`edigonzales/iox-cpp` und `.github/workflows/publish-iox.yml` zeigen. Vor dem
ersten Lauf muss der GitHub-Mirror denselben `main`-Commit wie Codeberg besitzen;
der Mirror sollte `main` als Default-Branch verwenden.
