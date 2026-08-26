# Build- und Publikationspipeline

`iox-cpp` wird im kanonischen GitHub-Repository entwickelt. Die GitHub Actions
laufen auf `main` und auf Pull Requests. Native Builds sind CI-Gates;
veröffentlicht werden Source und WASM.

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

Native SDKs werden zusätzlich über vcpkg verteilt. Das CMake-Package heißt
\`iox\`, der vcpkg-Port \`iox-cpp\`; die optionale Modellintegration wird über das
Feature \`ilic\` aktiviert. Die veröffentlichte Registry ist die gemeinsame
\`vcpkg-registry\`-Branch von \`ilic-fork\`, damit Downstream-Projekte nur eine
Registry konfigurieren müssen.

Die vcpkg-Pipeline publiziert immutable Snapshot- oder Stable-Versionen. Ein
Snapshot verwendet \`0.2.0-snapshot.<source-sha8>\`. Für jede Version werden
die eigenständigen Features \`ilic\` und \`geos\` sowie die Downstream-
Kombination \`ilic,geos\` für \`x64-linux\`, \`arm64-osx\`, \`x64-windows\` und
\`x64-windows-static\` gebaut und anschließend mit \`--only-binarycaching\`
wiederhergestellt. Die Binary-Pakete liegen im GitHub-NuGet-Feed
\`nuget.pkg.github.com/edigonzales\`.

Die vcpkg-Publikation wird nach einem erfolgreichen \`Publish iox-cpp\`-Lauf
über \`repository_dispatch\` angefordert. Der Workflow benötigt das Secret
\`VCPKG_REGISTRY_TOKEN\` mit Schreibzugriff auf die \`ilic-fork\`-Registry-Branch.
Downstream-GitHub-Actions benötigen Leserechte auf die GitHub-Packages.

## Trusted Publisher und Repository-Prüfung

Der npm Trusted Publisher für `@interlis/iox-wasm` muss auf
`edigonzales/iox-cpp` und `.github/workflows/publish-iox.yml` zeigen. Das
Repository muss `main` als Default-Branch verwenden; Veröffentlichungen werden
erst nach dem Merge des Release-Kandidaten nach `main` gestartet.
