# Implementierungs-Spezifikation für `iox-cpp`

## Auftrag an einen autonomen LLM-Coding-Client

Diese Spezifikation beschreibt den vollständigen Aufbau des neuen Git-Repositories **`iox-cpp`**. Sie ist als ausführbare Arbeitsanweisung für einen autonomen Coding-Client gedacht, insbesondere für ChatGPT/Codex-basierte Coding-Agents und OpenCode. Der Agent arbeitet das Vorhaben phasenweise, ohne menschliche Eingriffe und ohne Rückfragen ab.

Das Repository ist beim Start bereits vorhanden. Der Agent darf es **nicht neu anlegen**, keinen Remote konfigurieren und nichts pushen. CI/CD-Pipelines und Release-Automation sind ausdrücklich zulässig, wenn sie vom Auftrag verlangt werden; sie müssen dem kanonischen GitHub-Repository und dem unten beschriebenen Source/WASM-Releasevertrag folgen. Jede abgeschlossene Phase endet mit einem getesteten, praktisch nutzbaren Artefakt und einem eigenen Git-Commit.

Der funktionale Fokus liegt ausschliesslich auf einem modernen, nativen und WebAssembly-tauglichen **INTERLIS-XTF-Reader/Writer-Framework**. INTERLIS 1 beziehungsweise ITF ist ausdrücklich nicht Bestandteil dieses Vorhabens.

---

# 1. Verbindliche Produktziele

`iox-cpp` soll:

1. XTF 2.3 vollständig lesen und schreiben.
2. XTF 2.4 vollständig lesen und schreiben.
3. einen generischen, modellfreien Betrieb ermöglichen.
4. optional modellgestützte Verarbeitung über eine direkte Integration mit `ilic-core` anbieten.
5. ein ereignisbasiertes Streaming-API analog zum fachlichen Konzept von Java `iox-api` bereitstellen.
6. intern moderne C++17-Mittel verwenden:
   - `std::variant`,
   - RAII,
   - Value Types beziehungsweise value-ähnliche Handles,
   - keine manuelle Speicherfreigabe im öffentlichen C++-API.
7. nativ auf macOS ARM64, Linux x86_64 und Windows x86_64 kompilierbar sein.
8. mit Emscripten 3.1.64 nach WebAssembly kompilierbar sein.
9. in Browsern, Web Workern und Node.js ab Version 18 funktionieren.
10. keine DOM-Abhängigkeit in JavaScript voraussetzen.
11. weitere Reader und Writer über C++-Interfaces und eine explizite Registry erweiterbar machen.
12. eine stabile C-ABI für WebAssembly und spätere Sprachbindings bereitstellen.
13. ein lokales npm-Paket unter `packages/iox-wasm` mit dem Paketnamen `@interlis/iox-wasm` enthalten.
14. mindestens 90 Prozent Line Coverage und mindestens 85 Prozent Branch Coverage für die Kernbibliotheken erreichen.
15. Roundtrip-, Negativ-, Grenzwert-, Unicode-, Fuzz- und Native/WASM-Paritätstests enthalten.
16. eine ausführliche, agentenlesbare Projektsteuerung über `AGENTS.md` und repo-lokale Skills bereitstellen.
17. Verwende möglichst viele Tests und Testdaten aus https://github.com/claeis/iox-ili als Gegenprüfung für Reader und Writer.

---

# 2. Explizite Nicht-Ziele

Folgendes darf in diesem Vorhaben nicht implementiert werden:

- ITF Reader oder Writer;
- INTERLIS-1-Datentransfer;
- CSV als offizielles Austauschformat;
- eCH-0118/GML Reader oder Writer;
- vollständiger INTERLIS-Datenvalidator nach Art von `ilivalidator`;
- GEOS-, JTS- oder GDAL-Geometriekonvertierung;
- dynamische Plugins mit `dlopen`, DLL-Ladung oder ähnlichem;
- eine grafische Benutzeroberfläche;
- eine produktive Kommandozeilenanwendung in den frühen Phasen;
- byte-identische XML-Roundtrips;
- Bewahrung von XML-Kommentaren und Processing Instructions;
- ungefragte CI/CD-Konfigurationen oder Veröffentlichungen außerhalb des dokumentierten Releasevertrags;
- automatisches Pushen von Commits;
- direkte Übernahme des historischen IOM-Codes.

Ein kleiner `JsonEventReader` und `JsonEventWriter` ist ausdrücklich zulässig, aber nur als Test-, Demonstrations- und Erweiterbarkeitsformat. Er ist kein offizielles INTERLIS-Transferformat.

---

# 3. Normative und informative Referenzen

Die Implementierung muss sich in dieser Prioritätsreihenfolge orientieren:

1. **Normativ:** INTERLIS-2.3- und INTERLIS-2.4-Referenzhandbücher, insbesondere die Regeln für den sequentiellen Transfer und die XML-Codierung. https://www.interlis.ch/download/interlis2/ili2-refman_2006-04-13_d.pdf und https://www.interlis.ch/download/interlis2/STAN_d_DEF_2024-04-24_eCH-0031_V2.1.0_INTERLIS_2-Referenzhandbuch.pdf
2. **Verhaltensreferenz:** `claeis/iox-api` und `claeis/iox-ili`.
3. **Testreferenz:** offizielle INTERLIS-XTF-Test-Suite und geeignete öffentliche Testdatensätze.
4. **Ergänzende historische Referenz:** alter C/C++-IOM-Code im GDAL-Kontext.
5. **Compiler- und Metamodellreferenz:** `edigonzales/ilic-fork` und dessen `ilic-core`.

Bei Widersprüchen gilt die aktuelle normative XTF-Spezifikation. Jede bewusste Abweichung von `iox-ili` muss in `docs/conformance.md` dokumentiert und durch Tests abgesichert werden.

Verbindliche Referenzorte:

- https://www.interlis.ch/dokumentation/interlis-2
- https://github.com/claeis/iox-api
- https://github.com/claeis/iox-ili
- https://github.com/geoadmin/suite-interlis
- https://github.com/edigonzales/ilic-fork
- https://github.com/OSGeo/gdal/tree/master/ogr/ogrsf_frmts/ili
- https://github.com/libexpat/libexpat

Der Coding-Agent muss in Phase 0 die konkreten Versionsstände beziehungsweise Commit-SHAs der verwendeten Quellen in `docs/conformance.md` festhalten. Drittanbieterabhängigkeiten dürfen nie auf einem Floating Branch bleiben.

---

# 4. Verbindliche Architekturentscheidungen

## 4.1 Ereignisstrom ist der normative Kern

Der fachliche Transferstrom lautet:

```text
StartTransferEvent
  StartBasketEvent
    ObjectEvent*
  EndBasketEvent
EndTransferEvent
```

Der Reader produziert diesen Strom. Der Writer konsumiert denselben Strom. Alle weiteren APIs – `readAll()`, `readBasket()`, JavaScript-Iteratoren und JSON-Serialisierung – sind Convenience-Schichten über diesem Kern.

## 4.2 Generischer Betrieb ohne Modell

Ein XTF-Dokument muss grundsätzlich ohne kompiliertes INTERLIS-Modell gelesen werden können. Primitive Werte bleiben dabei in ihrer lexikalischen UTF-8-Repräsentation erhalten. Strukturen, Referenzen und Geometrien werden in generische IOM-Objekte überführt.

Für XTF 2.4 müssen XML-Namespace, lokaler XML-Name und abgeleiteter INTERLIS-Name separat gespeichert werden. Wo ein eindeutiger INTERLIS-Name ohne Modell nicht ableitbar ist, darf die Implementierung nicht raten oder Daten verlieren. Sie muss den expandierten XML-Namen bewahren und eine strukturierte Warnung erzeugen.

## 4.3 Direkte `ilic-core`-Integration, kein Provider-System

Es wird **keine** abstrakte Familie von Model Providern implementiert.

Die Kopplung wird so geschnitten:

```text
iox-core        modellunabhängige Objekte, Events und Interfaces
iox-xtf         generischer XTF-Reader/Writer
iox-json        Test-/Beispielformat
iox-abi         stabile C-ABI
iox-ilic        konkrete, direkte Integration mit ilic-core
```

`iox-ilic` linkt direkt gegen `ilic-core`. Es enthält konkrete Adapter- und Indexklassen für das `ilic`-Metamodell. Es gibt kein öffentliches `ModelProvider`- oder `TransferModelView`-Interface.

Der generische XTF-Kern darf nicht von `ilic-core` abhängen. Dadurch bleiben folgende Artefakte möglich:

- kleiner modellfreier Native Reader/Writer;
- kleines modellfreies WASM-Paket;
- optionales modellbewusstes Native- oder WASM-Bundle mit `ilic-core`.

Diese Trennung ist keine alternative Provider-Architektur. Sie ist lediglich eine Build- und Verantwortungsgrenze. Innerhalb von `iox-ilic` wird direkt mit den konkreten Typen von `ilic-core` gearbeitet.

## 4.4 Value-ähnliches `IomObject`

`IomObject` ist öffentlich ein kleiner, kopierbarer Handle. Intern verwendet es `std::shared_ptr<Impl>`. Mutierende Methoden führen vor einer Änderung `detach()` aus und erzeugen bei mehrfach referenziertem Zustand eine Kopie. Damit gelten nach aussen nachvollziehbare Value-Semantiken:

```cpp
IomObject a = ...;
IomObject b = a;
b.setPrimitive("Name", "neu");
// a bleibt unverändert
```

Es darf kein öffentliches manuelles `retain()`, `release()` oder `delete()` geben.

## 4.5 Reihenfolge bleibt erhalten

Die Implementierung muss bewahren:

- Reihenfolge der Attribute eines Objekts;
- Reihenfolge der Werte eines mehrwertigen Attributs;
- Reihenfolge der Objekte eines Baskets;
- Reihenfolge der Baskets;
- Reihenfolge bewahrter unbekannter fachlicher Erweiterungselemente.

`std::unordered_map` darf nicht als alleinige kanonische Speicherung geordneter Inhalte verwendet werden. Schnelle Lookup-Indizes dürfen ergänzend und invalidierbar geführt werden.

## 4.6 Roundtrip-Ziel

Reader → Eventstrom → Writer muss ein semantisch äquivalentes, deterministisch formatiertes XTF erzeugen.

Nicht verlangt werden:

- identische Einrückung;
- identische Namespace-Präfixe;
- identische XML-Attributreihenfolge, sofern semantisch irrelevant;
- identische Entity-Schreibweise;
- identische XML-Kommentare oder Processing Instructions;
- byte-identische Ausgabe.

## 4.7 Fehlerverhalten

- XML-Well-formedness-Fehler sind fatal.
- Verletzungen des XTF-Zustandsautomaten sind fatal.
- Fachliche Transferprobleme werden als strukturierte Diagnostics ausgegeben.
- Der optionale Strict Mode hebt definierte fachliche Fehler zu fatalen Fehlern an.
- Es gibt keine stillschweigenden Datenverluste.
- Unbekannte fachliche Elemente werden entweder generisch bewahrt oder ausdrücklich diagnostiziert.
- Keine C++-Exception darf eine C-ABI-Grenze überschreiten.

## 4.8 XML-Technologie

- Reader: Expat als gepinnte statische Abhängigkeit.
- Writer: eigener kleiner, kontrollierter UTF-8-XML-Writer.
- Keine DOM-Baumkonstruktion für das gesamte Transferdokument.
- Kein Xerces.
- Keine externe Entity-Auflösung.
- DTD-Verwendung wird abgelehnt.
- Reader muss inkrementell über Chunks betrieben werden können.

## 4.9 Format-Erweiterbarkeit

Weitere Formate werden über explizite C++-Interfaces und eine Registry integriert. Es gibt keine dynamische Plugin-Ladung. Registrierung geschieht explizit und testbar, nicht über globale statische Konstruktoren.

---

# 5. Zielstruktur des Repositories

Der Agent soll folgende Zielstruktur schrittweise aufbauen:

```text
iox-cpp/
├── AGENTS.md
├── CMakeLists.txt
├── LICENSE
├── README.md
├── THIRD_PARTY_NOTICES.md
├── .emscripten-version
├── .gitignore
├── cmake/
│   ├── IoxDependencies.cmake
│   ├── IoxOptions.cmake
│   ├── IoxSanitizers.cmake
│   ├── IoxCoverage.cmake
│   ├── IoxFuzzing.cmake
│   └── IoxWarnings.cmake
├── include/iox/
│   ├── Version.h
│   ├── Diagnostic.h
│   ├── Error.h
│   ├── ByteView.h
│   ├── IomName.h
│   ├── IomObject.h
│   ├── IomValue.h
│   ├── Events.h
│   ├── Reader.h
│   ├── Writer.h
│   ├── FormatRegistry.h
│   ├── Basket.h
│   ├── json/
│   │   ├── JsonEventReader.h
│   │   └── JsonEventWriter.h
│   ├── xtf/
│   │   ├── XtfVersion.h
│   │   ├── XtfReader.h
│   │   ├── XtfWriter.h
│   │   ├── XtfReaderOptions.h
│   │   ├── XtfWriterOptions.h
│   │   ├── XtfHeader.h
│   │   └── XtfNames.h
│   ├── ilic/
│   │   ├── IlicModelIndex.h
│   │   ├── IlicXtfReader.h
│   │   └── IlicXtfWriter.h
│   └── abi/
│       └── iox.h
├── source/
│   ├── core/
│   ├── json/
│   ├── xml/
│   ├── xtf/
│   │   ├── common/
│   │   ├── v23/
│   │   └── v24/
│   ├── ilic/
│   └── abi/
├── packages/
│   └── iox-wasm/
│       ├── package.json
│       ├── index.js
│       ├── index.d.ts
│       ├── worker.js
│       ├── README.md
│       ├── LICENSE
│       ├── THIRD_PARTY_NOTICES.md
│       └── test/
├── scripts/
│   ├── build-native.sh
│   ├── build-wasm.sh
│   ├── test-native.sh
│   ├── test-wasm.sh
│   ├── coverage.sh
│   ├── conformance.sh
│   └── differential-java.sh
├── examples/
│   ├── cpp-read-events.cpp
│   ├── cpp-roundtrip.cpp
│   ├── cpp-custom-format.cpp
│   ├── node-read-events.mjs
│   └── browser-worker/
├── tools/
│   └── iox-dump.cpp
├── test/
│   ├── support/
│   ├── core/
│   ├── json/
│   ├── xml/
│   ├── xtf23/
│   ├── xtf24/
│   ├── ilic/
│   ├── abi/
│   ├── parity/
│   ├── fuzz/
│   ├── corpus/
│   └── fixtures/
├── docs/
│   ├── architecture.md
│   ├── roadmap.md
│   ├── conformance.md
│   ├── event-json-schema.md
│   ├── extending-formats.md
│   ├── wasm.md
│   └── phase-status.md
└── .agents/
    └── skills/
        ├── architecture/SKILL.md
        ├── native-build/SKILL.md
        ├── wasm-build/SKILL.md
        ├── xtf-conformance/SKILL.md
        ├── testing/SKILL.md
        └── phase-execution/SKILL.md
```

Dateien werden erst in der Phase erstellt, in der sie benötigt werden. Es sollen keine leeren Platzhalterverzeichnisse ohne Nutzen entstehen.

---

# 6. Build-Stack und CMake-Anforderungen

## 6.1 Verbindliche Basis

Analog zu `ilic-fork`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(iox-cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
```

Verbindliche Plattformen:

- macOS ARM64 mit Clang;
- Linux x86_64 mit GCC oder Clang;
- Windows x86_64 mit MSVC;
- Emscripten 3.1.64;
- Node.js ab Version 18.

`.emscripten-version` enthält exakt:

```text
3.1.64
```

## 6.2 CMake-Optionen

Mindestens:

```cmake
option(IOX_BUILD_WASM "Build the WebAssembly ABI target" OFF)
option(IOX_ENABLE_ILIC "Build direct ilic-core integration" OFF)
option(IOX_FETCH_ILIC "Fetch the pinned ilic-fork dependency" OFF)
option(IOX_BUILD_EXAMPLES "Build examples" ON)
option(IOX_BUILD_TOOLS "Build integration tools" ON)
option(IOX_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
option(IOX_ENABLE_FUZZING "Build libFuzzer targets" OFF)
option(IOX_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(IOX_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(IOX_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
```

## 6.3 CMake-Targets

```text
iox-core                 STATIC
iox-json                 STATIC, linkt PUBLIC iox-core
iox-xml                  STATIC, intern
iox-xtf                  STATIC, linkt PUBLIC iox-core, PRIVATE iox-xml/expat
iox-abi                  STATIC, linkt PUBLIC iox-xtf und iox-json
iox-ilic                 STATIC, optional, linkt PUBLIC iox-xtf und ilic-core
iox-wasm                 Emscripten executable/module
iox-dump                 optionales Tool in späterer Phase
```

Aliases:

```cmake
add_library(iox::core ALIAS iox-core)
add_library(iox::json ALIAS iox-json)
add_library(iox::xtf ALIAS iox-xtf)
add_library(iox::abi ALIAS iox-abi)
add_library(iox::ilic ALIAS iox-ilic) # nur wenn aktiviert
```

Öffentliche Header dürfen keine Expat-Header enthalten. `iox-core` darf weder Expat noch `ilic-core` linken. `iox-xtf` darf `ilic-core` nicht linken. Nur `iox-ilic` besitzt diese direkte Abhängigkeit.

## 6.4 Abhängigkeiten

Expat wird mit `FetchContent` über einen exakten Commit oder Release-Tag eingebunden. In Phase 0 muss der Agent:

1. den aktuellen stabilen Expat-Release prüfen;
2. einen unveränderlichen Commit-SHA festhalten;
3. unnötige Expat-Tools, Beispiele und Tests deaktivieren;
4. statisches Linking erzwingen;
5. die Lizenz in `THIRD_PARTY_NOTICES.md` dokumentieren;
6. den Native- und Emscripten-Build verifizieren.

`ilic-fork` wird nicht als Git-Submodule eingebunden. Für `iox-ilic` gelten folgende Wege:

1. `IOX_ILIC_SOURCE_DIR` zeigt auf einen vorhandenen Checkout; oder
2. `IOX_FETCH_ILIC=ON` lädt einen in `IoxDependencies.cmake` fest gepinnten Commit; oder
3. später kann ein installiertes CMake-Package verwendet werden, sofern `ilic-fork` dies anbietet.

Die erste implementierte Variante muss mindestens Weg 1 und 2 unterstützen.

## 6.5 WASM-Ziel

Das Emscripten-Ziel folgt den Konventionen aus `ilic-fork`:

```cmake
target_link_options(iox-wasm PRIVATE
    "--no-entry"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sENVIRONMENT=web,worker,node"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sWASM_BIGINT=1"
    "-sASSERTIONS=1"
    "-fexceptions"
)
```

Exceptions dürfen intern verwendet werden, müssen aber vollständig innerhalb der ABI-Implementierung abgefangen werden. Exportiert werden ausschliesslich die explizit dokumentierten C-ABI-Funktionen.

`scripts/build-wasm.sh` muss wie beim `ilic-fork`:

- die gepinnte Emscripten-Version prüfen;
- optional einen benachbarten `emsdk`-Checkout automatisch initialisieren;
- stale CMake Toolchain Caches erkennen und entfernen;
- `build/wasm` verwenden;
- `.mjs` und `.wasm` nach `packages/iox-wasm` kopieren;
- bei falscher Version mit verständlicher Fehlermeldung abbrechen.

---

# 7. Öffentliche C++-API im Detail

## 7.1 Namensräume

```cpp
namespace iox { }
namespace iox::json { }
namespace iox::xtf { }
namespace iox::ilic { }
```

Öffentliche Typen verwenden keine Präfixe wie `Iox` innerhalb des `iox`-Namensraums, ausser der Begriff ist fachlich etabliert (`IoxEvent`, `IoxError`).

## 7.2 `ByteView`

Da C++17 kein `std::span` besitzt:

```cpp
class ByteView final {
public:
    constexpr ByteView() noexcept = default;
    constexpr ByteView(const std::uint8_t* data, std::size_t size) noexcept;
    ByteView(const std::string& value) noexcept;
    ByteView(const std::vector<std::uint8_t>& value) noexcept;

    const std::uint8_t* data() const noexcept;
    std::size_t size() const noexcept;
    bool empty() const noexcept;
    ByteView subview(std::size_t offset, std::size_t count) const;
};
```

`ByteView` besitzt keine Daten. Lebensdauerregeln müssen dokumentiert und getestet sein.

## 7.3 Diagnostics

```cpp
enum class DiagnosticSeverity {
    Info,
    Warning,
    Error,
    Fatal
};

enum class DiagnosticCode {
    XmlMalformed,
    XmlDtdForbidden,
    XmlExternalEntityForbidden,
    XmlLimitExceeded,
    UnexpectedElement,
    UnexpectedAttribute,
    InvalidEventOrder,
    InvalidXtfNamespace,
    UnsupportedXtfVersion,
    MissingRequiredHeader,
    MissingModelEntry,
    MissingBasketId,
    MissingObjectId,
    InvalidReference,
    InvalidGeometry,
    UnknownInterlisName,
    UnknownExtensionPreserved,
    ModelMismatch,
    WriterStateError,
    AbiInvalidArgument,
    InternalError
};

struct SourceLocation final {
    std::string sourceName;
    std::uint64_t byteOffset = 0;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

struct Diagnostic final {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticCode code = DiagnosticCode::InternalError;
    std::string message;
    SourceLocation location;
    std::vector<std::string> contextPath;
};

class DiagnosticSink {
public:
    virtual ~DiagnosticSink() = default;
    virtual void report(const Diagnostic& diagnostic) = 0;
};
```

Zusätzlich:

```cpp
class VectorDiagnosticSink final : public DiagnosticSink {
public:
    void report(const Diagnostic&) override;
    const std::vector<Diagnostic>& diagnostics() const noexcept;
    std::vector<Diagnostic> take();
    void clear() noexcept;
};
```

Diagnostics müssen stabile String-Codes in der JSON-Repräsentation besitzen, zum Beispiel `xml.malformed`, `xtf.invalid_event_order`. Die C++-Enum-Namen sind nicht die ABI.

## 7.4 Fataler Fehler

```cpp
class IoxError : public std::runtime_error {
public:
    IoxError(DiagnosticCode code,
             std::string message,
             SourceLocation location = {});

    DiagnosticCode code() const noexcept;
    const SourceLocation& location() const noexcept;
};
```

Fachliche Warnungen und nichtfatale Fehler werden nicht als Exceptions verwendet. XML-Well-formedness, Ressourcenlimits, ungültige Reader-/Writer-Zustände und interne Invarianten dürfen `IoxError` auslösen.

## 7.5 `IomName`

XTF 2.4 macht eine explizite Trennung zwischen fachlichem Namen und XML-QName notwendig:

```cpp
struct XmlQualifiedName final {
    std::string namespaceUri;
    std::string localName;
    std::string prefixHint;

    bool empty() const noexcept;
    std::string expanded() const;
};

class IomName final {
public:
    IomName() = default;
    explicit IomName(std::string interlisName);
    IomName(std::string interlisName, XmlQualifiedName xmlName);

    const std::string& interlisName() const noexcept;
    const XmlQualifiedName& xmlName() const noexcept;
    bool hasInterlisName() const noexcept;
    bool hasXmlName() const noexcept;

    static IomName fromExpandedXmlName(XmlQualifiedName name);
};
```

Regeln:

- Für XTF 2.3 ist `interlisName` normalerweise vollständig bekannt.
- Für XTF 2.4 soll der Reader bestmöglich einen fachlichen Namen ableiten.
- Der ursprüngliche expandierte XML-Name bleibt erhalten.
- Writer bevorzugt normative Namen aus `iox-ilic`, danach gespeicherte XML-Namen, danach deterministische Ableitung.
- Ist keine verlustfreie Ausgabe möglich, muss der Writer abbrechen; er darf keinen erfundenen Namespace schreiben.

## 7.6 IOM-Werte

Primitive Werte werden nicht automatisch in Zahlen, Booleans oder Datumswerte umgewandelt. Die lexikalische Repräsentation bleibt massgebend.

```cpp
class IomObject;

class IomValue final {
public:
    enum class Kind { Primitive, Object };

    static IomValue primitive(std::string value);
    static IomValue object(IomObject value);

    Kind kind() const noexcept;
    bool isPrimitive() const noexcept;
    bool isObject() const noexcept;

    const std::string& primitive() const;
    const IomObject& object() const;
    IomObject& object();
};
```

Intern kann `std::variant<std::string, IomObject>` verwendet werden.

## 7.7 Objektmetadaten

```cpp
enum class ObjectOperation {
    Insert,
    Update,
    Delete,
    None
};

enum class Consistency {
    Complete,
    Incomplete,
    Inconsistent,
    Adapted,
    Unspecified
};

struct ReferenceInfo final {
    std::optional<std::string> targetOid;
    std::optional<std::string> targetBasketId;
    std::optional<std::uint64_t> orderPosition;
};
```

## 7.8 `IomObject`

```cpp
class IomObject final {
public:
    IomObject();
    explicit IomObject(IomName tag,
                       std::optional<std::string> oid = std::nullopt);

    IomObject(const IomObject&) noexcept;
    IomObject(IomObject&&) noexcept;
    IomObject& operator=(const IomObject&) noexcept;
    IomObject& operator=(IomObject&&) noexcept;
    ~IomObject();

    bool empty() const noexcept;

    const IomName& tag() const;
    void setTag(IomName tag);

    const std::optional<std::string>& oid() const noexcept;
    void setOid(std::optional<std::string> oid);

    ObjectOperation operation() const noexcept;
    void setOperation(ObjectOperation operation);

    Consistency consistency() const noexcept;
    void setConsistency(Consistency consistency);

    const ReferenceInfo& reference() const noexcept;
    void setReference(ReferenceInfo reference);
    bool isReference() const noexcept;

    const SourceLocation& sourceLocation() const noexcept;
    void setSourceLocation(SourceLocation location);

    std::size_t attributeCount() const noexcept;
    const IomName& attributeName(std::size_t attributeIndex) const;
    bool hasAttribute(std::string_view interlisName) const;

    std::size_t valueCount(std::string_view interlisName) const;
    const IomValue& value(std::string_view interlisName,
                          std::size_t valueIndex) const;

    std::optional<std::string_view> primitive(
        std::string_view interlisName,
        std::size_t valueIndex = 0) const;

    std::optional<IomObject> object(
        std::string_view interlisName,
        std::size_t valueIndex = 0) const;

    void setPrimitive(IomName attribute,
                      std::string value);
    void appendPrimitive(IomName attribute,
                         std::string value);
    void setObject(IomName attribute,
                   IomObject value);
    void appendObject(IomName attribute,
                      IomObject value);

    void insertValue(IomName attribute,
                     std::size_t valueIndex,
                     IomValue value);
    void replaceValue(std::string_view interlisName,
                      std::size_t valueIndex,
                      IomValue value);
    void eraseValue(std::string_view interlisName,
                    std::size_t valueIndex);
    void eraseAttribute(std::string_view interlisName);
    void clearAttributes();

    IomObject deepCopy() const;
    bool semanticallyEquals(const IomObject& other) const;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    void detach();
};
```

### Interne Speicherung

`Impl` enthält mindestens:

```cpp
struct AttributeEntry {
    IomName name;
    std::vector<IomValue> values;
};

struct IomObject::Impl {
    IomName tag;
    std::optional<std::string> oid;
    ObjectOperation operation = ObjectOperation::None;
    Consistency consistency = Consistency::Unspecified;
    ReferenceInfo reference;
    SourceLocation location;
    std::vector<AttributeEntry> attributes;
    mutable std::unordered_map<std::string, std::size_t> lookup;
    mutable bool lookupValid = false;
};
```

`lookup` ist nur ein Cache. `attributes` bleibt die kanonische Reihenfolge. Jede Mutation invalidiert den Lookup-Cache.

### COW-Regeln

- Jeder mutierende Einstieg ruft `detach()` auf.
- Es gibt keinen öffentlichen mutablen Verweis auf einen intern gespeicherten `IomValue`, weil dies COW umgehen würde. Änderungen erfolgen über `set*`, `append*`, `insertValue`, `replaceValue` und `erase*`.
- `detach()` kopiert `Impl`, wenn `!impl_.unique()`.
- Die Kopie darf strukturierte Kindobjekte zunächst teilen, da deren eigene Mutation wiederum COW auslöst.
- `deepCopy()` kopiert rekursiv die gesamte Objektstruktur.
- Zyklen in IOM-Strukturen sind unzulässig. Debug Builds und JSON-Serialisierung müssen Zyklenerkennung besitzen.

## 7.9 Header-Typen

```cpp
enum class XtfVersion { V23, V24 };

struct ModelEntry final {
    std::string name;
    std::optional<std::string> version;
    std::optional<std::string> uri;
    XmlQualifiedName xmlNamespace;
};

struct OidSpace final {
    std::string name;
    std::string domain;
};

struct ExtensionAttribute final {
    XmlQualifiedName name;
    std::string value;
};

struct ExtensionElement final {
    XmlQualifiedName name;
    std::vector<ExtensionAttribute> attributes;
    std::string text;
    std::vector<ExtensionElement> children;
};

struct TransferHeader final {
    XtfVersion version = XtfVersion::V23;
    std::string sender;
    std::optional<std::string> comment;
    std::vector<ModelEntry> models;
    std::vector<OidSpace> oidSpaces;
    std::vector<ExtensionElement> extensions;
};
```

`ExtensionElement` wird nur für unbekannte fachliche Elemente verwendet. XML-Kommentare und Processing Instructions werden nicht gespeichert.

## 7.10 Basket-Typen

```cpp
enum class BasketKind {
    Full,
    Update,
    Initial,
    Unspecified
};

struct BasketMetadata final {
    IomName topic;
    std::string basketId;
    BasketKind kind = BasketKind::Unspecified;
    Consistency consistency = Consistency::Unspecified;
    std::optional<std::string> startState;
    std::optional<std::string> endState;
    std::vector<std::string> domains;
    std::vector<std::string> topics;
    std::vector<ExtensionElement> extensions;
    SourceLocation location;
};
```

## 7.11 Events

```cpp
struct StartTransferEvent final {
    TransferHeader header;
};

struct StartBasketEvent final {
    BasketMetadata basket;
};

struct ObjectEvent final {
    IomObject object;
};

struct EndBasketEvent final { };
struct EndTransferEvent final { };

using IoxEvent = std::variant<
    StartTransferEvent,
    StartBasketEvent,
    ObjectEvent,
    EndBasketEvent,
    EndTransferEvent
>;
```

Hilfsfunktionen:

```cpp
enum class EventKind {
    StartTransfer,
    StartBasket,
    Object,
    EndBasket,
    EndTransfer
};

EventKind eventKind(const IoxEvent&) noexcept;
std::string_view eventKindName(EventKind) noexcept;
```

## 7.12 Reader-API

Zwei Ebenen sind erforderlich.

### Allgemeines Format-Interface

```cpp
enum class ReaderProgress {
    Event,
    NeedInput,
    End
};

struct ReadOutcome final {
    ReaderProgress progress = ReaderProgress::NeedInput;
    std::optional<IoxEvent> event;
};

class Reader {
public:
    virtual ~Reader() = default;

    virtual ReadOutcome next() = 0;
    virtual void feed(ByteView chunk) = 0;
    virtual void finish() = 0;
    virtual bool isFinished() const noexcept = 0;
    virtual std::vector<Diagnostic> takeDiagnostics() = 0;
};
```

Vertragsregeln:

- `feed()` darf einen Chunk sofort verarbeiten oder intern referenzfrei kopieren.
- Nach Rückkehr aus `feed()` darf der Caller seinen Puffer freigeben.
- `next()` liefert nie ein leeres `event`, wenn `progress == Event`.
- `finish()` signalisiert Ende der Eingabe und darf genau einmal aufgerufen werden.
- `feed()` nach `finish()` ist ein fataler Zustandsfehler.
- `next()` nach `End` bleibt idempotent `End`.
- Fatalfehler setzen den Reader in einen terminalen Fehlerzustand.

### Bequeme Stream-Schicht

```cpp
class StreamReader final {
public:
    StreamReader(std::unique_ptr<Reader> reader,
                 std::istream& input,
                 std::size_t chunkSize = 64 * 1024);

    std::optional<IoxEvent> read();
    std::vector<Diagnostic> takeDiagnostics();
};
```

`read()` füttert bei `NeedInput` automatisch weiter und gibt `std::nullopt` nach `End` zurück.

## 7.13 Writer-API

```cpp
class OutputSink {
public:
    virtual ~OutputSink() = default;
    virtual void write(ByteView bytes) = 0;
    virtual void flush() = 0;
};

class OstreamOutputSink final : public OutputSink {
public:
    explicit OstreamOutputSink(std::ostream& output);
    void write(ByteView bytes) override;
    void flush() override;
};

class VectorOutputSink final : public OutputSink {
public:
    void write(ByteView bytes) override;
    void flush() override;
    const std::vector<std::uint8_t>& bytes() const noexcept;
    std::vector<std::uint8_t> take();
};

class Writer {
public:
    virtual ~Writer() = default;
    virtual void write(const IoxEvent& event) = 0;
    virtual void flush() = 0;
    virtual void close() = 0;
    virtual bool isClosed() const noexcept = 0;
    virtual std::vector<Diagnostic> takeDiagnostics() = 0;
};
```

`Writer` erzwingt den Eventzustandsautomaten. `close()` ist idempotent, darf aber fehlende End-Events nicht erfinden. Wird ein Writer zerstört, bevor er korrekt geschlossen wurde, darf der Destruktor nicht werfen; im Debug Build soll eine Assertion beziehungsweise ein klarer interner Status verfügbar sein.

## 7.14 Basket-Convenience

```cpp
class Basket final {
public:
    BasketMetadata metadata;
    std::vector<IomObject> objects;
};

class BasketReader final {
public:
    explicit BasketReader(std::unique_ptr<Reader> reader,
                          std::size_t maxObjectsPerBasket = 0);

    const TransferHeader& header();
    std::optional<Basket> readBasket();
    std::vector<Diagnostic> takeDiagnostics();
};
```

- `header()` liest bei Bedarf bis `StartTransferEvent`.
- `readBasket()` konsumiert genau einen vollständigen Basket.
- `maxObjectsPerBasket == 0` bedeutet kein künstliches Limit.
- Wird ein Limit überschritten, entsteht ein fataler `XmlLimitExceeded`-ähnlicher Ressourcenfehler mit eigenem stabilen Code.
- Diese Klasse darf erst in einer späteren Phase entstehen; der Eventstream bleibt die Quelle der Wahrheit.

---

# 8. Erweiterbare Formatarchitektur

## 8.1 Formatbeschreibung

```cpp
struct FormatDescriptor final {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<std::string> mimeTypes;
    bool canRead = false;
    bool canWrite = false;
};
```

Formatnamen sind lower-case ASCII und stabil, zum Beispiel `xtf` und `json-events`.

## 8.2 Factory-Funktionen

```cpp
using ReaderCreator = std::function<std::unique_ptr<Reader>()>;
using WriterCreator = std::function<std::unique_ptr<Writer>(
    std::shared_ptr<OutputSink>)>;
using Sniffer = std::function<int(ByteView)>;
```

Der Sniffer liefert 0 bis 100. 0 bedeutet „kein Treffer“. Ein Treffer über Dateiendung allein darf nicht höher als ein eindeutiger Content-Sniff sein.

## 8.3 `FormatRegistry`

```cpp
class FormatRegistry final {
public:
    void registerReader(FormatDescriptor descriptor,
                        ReaderCreator creator,
                        Sniffer sniffer = {});

    void registerWriter(FormatDescriptor descriptor,
                        WriterCreator creator);

    bool contains(std::string_view formatName) const;
    std::vector<FormatDescriptor> formats() const;

    std::unique_ptr<Reader> createReaderByName(
        std::string_view formatName) const;

    std::unique_ptr<Reader> createReader(
        std::string_view sourceName,
        ByteView prefix) const;

    std::unique_ptr<Writer> createWriterByName(
        std::string_view formatName,
        std::shared_ptr<OutputSink> output) const;
};
```

Anforderungen:

- Doppelte Formatnamen sind ein Konfigurationsfehler.
- Registrierung ist explizit; keine statischen globalen Registrar-Objekte.
- Registry behält Registrierungsreihenfolge für deterministisches Tie-Breaking.
- `defaultFormatRegistry()` verwendet ein thread-safe function-local static und registriert nur offizielle Built-ins.
- `json-events` wird in der Default Registry nur eingebunden, wenn `IOX_ENABLE_JSON_FORMAT` aktiviert ist.

## 8.4 Convenience-Facades

```cpp
class ReaderFactory final {
public:
    static std::unique_ptr<Reader> create(
        std::string_view sourceName,
        ByteView prefix = {});

    static std::unique_ptr<Reader> createByName(
        std::string_view formatName);
};

class WriterFactory final {
public:
    static std::unique_ptr<Writer> create(
        std::string_view formatName,
        std::shared_ptr<OutputSink> output);
};
```

Die vom Benutzer gewünschte Semantik:

```cpp
auto reader = ReaderFactory::create("data.xtf", prefix);
auto writer = WriterFactory::create("xtf", output);
```

muss als dokumentiertes Beispiel und Test existieren.

---

# 9. JSON-Eventformat als Erweiterbarkeitsbeweis

Das JSON-Eventformat ist newline-delimited JSON oder ein JSON-Array; die konkrete Wahl muss früh festgelegt und in `docs/event-json-schema.md` dokumentiert werden. Für Streaming ist NDJSON zu bevorzugen.

Jede Zeile repräsentiert exakt ein Event:

```json
{"schema":"iox-event/1","event":"startTransfer","header":{...}}
{"schema":"iox-event/1","event":"startBasket","basket":{...}}
{"schema":"iox-event/1","event":"object","object":{...}}
{"schema":"iox-event/1","event":"endBasket"}
{"schema":"iox-event/1","event":"endTransfer"}
```

Die JSON-Repräsentation ist gleichzeitig das stabile Schema an der C-ABI-Grenze.

## 9.1 Objekt-JSON

```json
{
  "tag": {
    "interlisName": "Model.Topic.Class",
    "xml": {
      "namespaceUri": "...",
      "localName": "...",
      "prefixHint": "Model"
    }
  },
  "oid": "123",
  "operation": "none",
  "consistency": "complete",
  "reference": null,
  "attributes": [
    {
      "name": {"interlisName":"Name","xml":null},
      "values": [
        {"kind":"primitive","value":"Müller"}
      ]
    }
  ]
}
```

Attribute sind ein Array, nie ein JSON-Objekt, damit die Reihenfolge erhalten bleibt.

## 9.2 Klassen

```cpp
class JsonEventWriter final : public Writer {
public:
    explicit JsonEventWriter(std::shared_ptr<OutputSink> output);
    void write(const IoxEvent&) override;
    void flush() override;
    void close() override;
    bool isClosed() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;
};

class JsonEventReader final : public Reader {
public:
    explicit JsonEventReader(JsonReaderOptions options = {});
    ReadOutcome next() override;
    void feed(ByteView) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;
};
```

Dieses Format wird genutzt, um bereits vor dem XTF-Parser zu beweisen:

- Eventzustandsautomat;
- IOM-Objektmodell;
- Reader/Writer-Erweiterbarkeit;
- C-ABI;
- WASM-Transport;
- Native/WASM-Parität;
- Roundtrip-Vergleich.

Es darf keine versteckte Produktionsabhängigkeit des XTF-Kerns auf einen grossen JSON-Parser entstehen. Die Implementierung darf einen kleinen, gepinnten JSON-Baustein verwenden oder ein kontrolliertes internes JSON-Modul bereitstellen. Die Entscheidung wird in Phase 1 dokumentiert. Falls eine Drittbibliothek verwendet wird, gelten dieselben Pinning- und Lizenzregeln wie für Expat.

---

# 10. Interne XML-Grundarchitektur

## 10.1 Sicherheitsregeln

Der Reader muss standardmässig:

- DTD-Deklarationen ablehnen;
- externe Entities ablehnen;
- externe Parameter Entities ablehnen;
- keine Netzwerkzugriffe ausführen;
- Entity-Expansion ausser den vordefinierten XML-Entities nicht zulassen;
- UTF-8 validieren;
- Zeile, Spalte und Byte-Offset aus Expat übernehmen;
- konfigurierbare Grenzen für Tiefe, Attribute, Textlänge und Gesamtinput besitzen.

```cpp
struct XmlLimits final {
    std::size_t maxDepth = 256;
    std::size_t maxAttributesPerElement = 1024;
    std::size_t maxTextBytesPerNode = 16 * 1024 * 1024;
    std::size_t maxTotalInputBytes = 0;
    std::size_t maxQueuedEvents = 1024;
};
```

`0` bedeutet für `maxTotalInputBytes` unbegrenzt. Andere Limits dürfen nicht mit `0` deaktiviert werden, ohne dass dies ausdrücklich dokumentiert ist.

## 10.2 XML-Namen und Attribute

```cpp
struct XmlAttribute final {
    XmlQualifiedName name;
    std::string value;
};

struct XmlStartElement final {
    XmlQualifiedName name;
    std::vector<XmlAttribute> attributes;
    SourceLocation location;
};

struct XmlEndElement final {
    XmlQualifiedName name;
    SourceLocation location;
};
```

Expat soll mit Namespace-Verarbeitung initialisiert werden. Der Namespace-Separator muss ein Zeichen sein, das in XML-Namen nicht vorkommt. Die Trennung in URI und Local Name erfolgt zentral in `XmlNameCodec`.

## 10.3 `ExpatParser`

```cpp
class ExpatParser final {
public:
    using StartHandler = std::function<void(const XmlStartElement&)>;
    using EndHandler = std::function<void(const XmlEndElement&)>;
    using TextHandler = std::function<void(std::string_view,
                                           const SourceLocation&)>;

    explicit ExpatParser(XmlLimits limits = {});
    ~ExpatParser();

    void setStartHandler(StartHandler);
    void setEndHandler(EndHandler);
    void setTextHandler(TextHandler);

    void feed(ByteView bytes);
    void finish();
    bool finished() const noexcept;
    SourceLocation location() const;
};
```

Anforderungen:

- Expat-Ressource über RAII freigeben.
- Callback-Ausnahmen innerhalb der C-Callbacks auffangen und nach `XML_StopParser` kontrolliert in C++ weiterreichen.
- Kein Throw durch einen C-Callback-Frame.
- `feed()` akzeptiert beliebige Chunk-Grenzen, auch mitten in UTF-8-Sequenzen, Entity-Referenzen, Elementnamen und Textknoten.
- Textcallbacks dürfen mehrfach für denselben logischen Textinhalt kommen; `TextAccumulator` führt sie kontrolliert zusammen.
- `finish()` meldet unvollständige Dokumente zuverlässig.

## 10.4 `XmlWriter`

```cpp
struct XmlWriterOptions final {
    bool pretty = true;
    std::string indent = "  ";
    std::string newline = "\n";
    bool writeDeclaration = true;
};

class XmlWriter final {
public:
    XmlWriter(std::shared_ptr<OutputSink> output,
              XmlWriterOptions options = {});

    void startDocument();
    void startElement(const XmlQualifiedName& name);
    void writeNamespace(std::string_view prefix,
                        std::string_view namespaceUri);
    void writeAttribute(const XmlQualifiedName& name,
                        std::string_view value);
    void text(std::string_view value);
    void endElement();
    void endDocument();
    void flush();
};
```

Regeln:

- Nur UTF-8.
- XML-Deklaration exakt `<?xml version="1.0" encoding="UTF-8"?>`.
- Attribute werden korrekt escaped.
- Text wird korrekt escaped.
- Ungültige XML-Zeichen werden abgelehnt.
- `]]>` wird sicher behandelt.
- Namespace-Präfixbindungen werden pro Scope verwaltet.
- Der Writer darf keine ungültigen doppelten Attribute erzeugen.
- Namespace-Präfixe werden deterministisch zugewiesen.
- Destruktor wirft nicht.

## 10.5 XML-Ereignisse sind nicht das öffentliche API

Die XML-Schicht bleibt intern. Öffentliche Benutzer sehen ausschliesslich Iox Events und IOM-Objekte. So können Parserdetails später geändert werden, ohne die API zu brechen.

---

# 11. XTF-Reader-Architektur

## 11.1 Öffentliche Optionen

```cpp
enum class Strictness {
    Lenient,
    Strict
};

struct XtfReaderOptions final {
    Strictness strictness = Strictness::Lenient;
    XmlLimits xmlLimits;
    std::string sourceName;
    bool preserveUnknownExtensions = true;
    bool requireAtLeastOneModel = true;
    bool allowVersionAutoDetection = true;
    std::optional<XtfVersion> expectedVersion;
};
```

## 11.2 Öffentliche Klasse

```cpp
class XtfReader final : public Reader {
public:
    explicit XtfReader(XtfReaderOptions options = {});
    ~XtfReader() override;

    ReadOutcome next() override;
    void feed(ByteView chunk) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

    std::optional<XtfVersion> detectedVersion() const noexcept;
};
```

`XtfReader` verwendet PImpl, damit Expat und interne Dialektklassen nicht in öffentliche Header gelangen.

## 11.3 Zustandsautomat

```cpp
enum class XtfReadState {
    BeforeDocument,
    BeforeTransfer,
    InHeader,
    AfterHeader,
    InDataSection,
    InBasket,
    InObject,
    AfterDataSection,
    AfterTransfer,
    Finished,
    Failed
};
```

Der Zustandsautomat muss zentral implementiert und separat unit-getestet werden. Dialektspezifische Parser dürfen nicht eigenständig widersprüchliche Zustände führen.

## 11.4 Dialektgrenze

XTF 2.3 und 2.4 sind intern getrennte Implementierungen:

```cpp
class XtfDialect {
public:
    virtual ~XtfDialect() = default;
    virtual XtfVersion version() const noexcept = 0;
    virtual void onStartElement(const XmlStartElement&) = 0;
    virtual void onEndElement(const XmlEndElement&) = 0;
    virtual void onText(std::string_view,
                        const SourceLocation&) = 0;
    virtual std::optional<IoxEvent> takeEvent() = 0;
    virtual bool finished() const noexcept = 0;
};

class Xtf23Dialect final : public XtfDialect { ... };
class Xtf24Dialect final : public XtfDialect { ... };
```

`XtfDialect` ist intern und keine Erweiterungsschnittstelle. `XtfReader` erkennt die Version am Root-Namespace und konstruiert den passenden Dialekt.

## 11.5 Ereignis-Queue und Backpressure

Expat kann innerhalb eines Chunks mehrere Iox Events erzeugen. `XtfReader` besitzt eine begrenzte Queue:

```cpp
std::deque<IoxEvent> pendingEvents_;
```

- Sobald `maxQueuedEvents` erreicht ist, pausiert der Parser kontrolliert.
- `next()` entnimmt ein Event und setzt den Parser fort, soweit bereits gepufferter Input vorhanden ist.
- Es darf nicht die gesamte Datei in Events vorgelesen werden.
- Chunk-Eingabe und Eventausgabe müssen unabhängig getestet werden.

## 11.6 Namensauflösung

Interne Klasse:

```cpp
class XtfNameCodec final {
public:
    static IomName decode23Element(const XmlQualifiedName& xmlName);
    static IomName decode24Class(const XmlQualifiedName& xmlName,
                                 const TransferHeader& header,
                                 DiagnosticSink& diagnostics);
    static IomName decode24Property(const XmlQualifiedName& xmlName,
                                    const IomName& owner,
                                    DiagnosticSink& diagnostics);

    static XmlQualifiedName encode23(const IomName& name);
    static XmlQualifiedName encode24(const IomName& name,
                                     const NamespaceTable& namespaces);
};
```

`NamespaceTable` wird aus Headerinformationen, gelesenen QNames und optional `iox-ilic` aufgebaut. Modellfreier Roundtrip muss gespeicherte XML-Namen verwenden.

## 11.7 Objekt-Builder

```cpp
class IomObjectBuilder final {
public:
    void beginObject(IomName tag,
                     std::optional<std::string> oid,
                     SourceLocation location);
    void beginAttribute(IomName name);
    void addPrimitive(std::string value);
    void addStructured(IomObject value);
    IomObject endObject();
    bool active() const noexcept;
};
```

Für rekursive Strukturen verwendet der Dialekt einen Stack von Builder Frames. Der Stack ist durch `maxDepth` begrenzt.

## 11.8 XTF 2.3 Reader

Der XTF-2.3-Reader muss mindestens verarbeiten:

- Root `TRANSFER` im Namespace `http://www.interlis.ch/INTERLIS2.3`;
- `HEADERSECTION` mit `VERSION`, `SENDER`, optionalem Kommentar;
- `MODELS/MODEL` und zugehörige Angaben;
- OID Spaces;
- Alias-Tabellen, soweit die XTF-2.3-Spezifikation sie verwendet;
- `DATASECTION`;
- Topic-Baskets mit `BID`;
- Basket Kind, Consistency, Start State, End State und Domains/Topics;
- Objekte mit `TID`;
- Operation und Consistency;
- primitive Attribute;
- Strukturen und mehrwertige Attribute;
- Referenzen mit `REF`, optional `BID` und `ORDER_POS`;
- Delete-Operationen entsprechend der Spezifikation;
- Geometrien gemäss Abschnitt 13;
- unbekannte fachliche Elemente gemäss Extension Policy.

Ein gültiger XTF-2.3-Reader darf kein Modell benötigen, um den Eventstrom und die generische Objektstruktur zu erzeugen.

## 11.9 XTF 2.4 Reader

Der XTF-2.4-Reader muss mindestens verarbeiten:

- Root `ili:transfer` im Namespace `http://www.interlis.ch/xtf/2.4/INTERLIS`;
- separate INTERLIS- und Geometry-Namespaces;
- `headersection`, `models`, `model`, `sender`, `comment`;
- pro Modell deklarierte XML-Namespaces;
- `datasection`;
- Basket- und Objekt-QNames mit Modellnamespace;
- `ili:bid`, `ili:tid`, `ili:ref`, `ili:order_pos`;
- `ili:operation`, `ili:consistency`, `ili:kind`;
- `ili:startstate`, `ili:endstate`, `ili:domains`;
- `ili:delete`;
- XTF-2.4-Geometrieelemente im Geometry Namespace;
- Mehrfachgeometrien;
- unbekannte fachliche Elemente gemäss Extension Policy.

Die Versionserkennung darf nicht ausschliesslich auf Dateiendungen beruhen.

## 11.10 Extension Policy

Bei unbekannten Elementen:

1. Ist das Element innerhalb eines normalen Objekts als generische Eigenschaft darstellbar, wird es als `IomName` plus `IomValue` bewahrt.
2. Ist es ein unbekanntes Header- oder Basket-Steuerelement, wird bei `preserveUnknownExtensions=true` ein `ExtensionElement` erzeugt.
3. Der Reader erzeugt `UnknownExtensionPreserved` als Warning.
4. Bei `preserveUnknownExtensions=false` ist das Element im Lenient Mode ein Error-Diagnostic und im Strict Mode fatal.
5. Kommentare und Processing Instructions werden ignoriert, ohne eine Datenverlustwarnung.

## 11.11 Strict Mode

Strict Mode umfasst mindestens:

- erforderliche Headerfelder;
- mindestens ein Model Entry, sofern konfiguriert;
- korrekte Eventreihenfolge;
- erforderliche BID/TID-Regeln;
- nur bekannte XTF-Steuerattribute;
- gültige Konsistenz-, Kind- und Operationswerte;
- Geometriestruktur;
- eindeutige Namespace-Abbildung;
- mit `iox-ilic`: bekannte Klassen, Topics, Attribute und modellkonforme Namensabbildung.

Strict Mode ist noch kein vollständiger INTERLIS-Datenvalidator. Kardinalitäten, Constraints, Eindeutigkeit und Referenzintegrität über die gesamte Datei sind ausserhalb des Kerns, sofern sie nicht für korrektes Parsing benötigt werden.

---

# 12. XTF-Writer-Architektur

## 12.1 Optionen

```cpp
struct XtfWriterOptions final {
    XtfVersion version = XtfVersion::V23;
    Strictness strictness = Strictness::Lenient;
    XmlWriterOptions xml;
    bool preserveUnknownExtensions = true;
    bool deterministicPrefixes = true;
    std::string defaultSender = "iox-cpp";
};
```

## 12.2 Öffentliche Klasse

```cpp
class XtfWriter final : public Writer {
public:
    XtfWriter(std::shared_ptr<OutputSink> output,
              XtfWriterOptions options = {});
    ~XtfWriter() override;

    void write(const IoxEvent& event) override;
    void flush() override;
    void close() override;
    bool isClosed() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;
};
```

PImpl ist Pflicht.

## 12.3 Zustandsautomat

```cpp
enum class XtfWriteState {
    BeforeTransfer,
    InTransfer,
    InBasket,
    AfterBasket,
    AfterTransfer,
    Closed,
    Failed
};
```

Erlaubte Übergänge:

```text
BeforeTransfer --StartTransfer--> InTransfer
InTransfer     --StartBasket----> InBasket
InBasket       --Object---------> InBasket
InBasket       --EndBasket------> AfterBasket
AfterBasket    --StartBasket----> InBasket
InTransfer     --EndTransfer----> AfterTransfer
AfterBasket    --EndTransfer----> AfterTransfer
AfterTransfer  --close----------> Closed
```

Jeder andere Übergang ist fatal mit `WriterStateError`. Der Writer darf keine Events ergänzen oder automatisch umsortieren.

## 12.4 Deterministische Ausgabe

Bei gleichen Events und gleichen Optionen muss exakt dieselbe Bytefolge erzeugt werden.

Festzulegen und zu testen:

- UTF-8;
- LF als Newline;
- zwei Spaces Einrückung;
- XML-Deklaration;
- keine unnötigen leeren Namespace-Deklarationen;
- stabile Präfixe:
  - `ili` für den XTF-2.4-INTERLIS-Namespace;
  - `geom` für den Geometry Namespace;
  - `xsi` für XML Schema Instance;
  - Modellpräfix primär Modellname, bei Kollision deterministisch mit numerischem Suffix;
- Headerreihenfolge gemäss Spezifikation;
- Objektattribute in gespeicherter beziehungsweise modellgestützter Reihenfolge;
- keine Locale-abhängige Formatierung.

Primitive Werte werden als Strings geschrieben. Der Writer nimmt keine numerischen Normalisierungen vor.

## 12.5 Modellfreies Schreiben

Modellfreies Schreiben ist erlaubt. Dabei gelten folgende Regeln:

1. Für XTF 2.3 genügt normalerweise der fachliche INTERLIS-Name.
2. Für XTF 2.4 muss entweder ein gespeicherter XML-QName oder eine eindeutige Namespace-Abbildung aus dem Header vorhanden sein.
3. Der Writer schreibt unbekannte Klassen und Attribute, wenn ihre XML-Namen verlustfrei bestimmt sind.
4. Fehlt die Namensinformation, wird nicht geraten. Der Writer erzeugt einen fatalen Fehler.
5. Gespeicherte Attributreihenfolge wird verwendet.
6. Der Writer darf im Strict Mode zusätzliche `iox-ilic`-Prüfungen verwenden.

## 12.6 Writer-Dialekte

```cpp
class XtfWriterDialect {
public:
    virtual ~XtfWriterDialect() = default;
    virtual void writeStartTransfer(const StartTransferEvent&) = 0;
    virtual void writeStartBasket(const StartBasketEvent&) = 0;
    virtual void writeObject(const ObjectEvent&) = 0;
    virtual void writeEndBasket(const EndBasketEvent&) = 0;
    virtual void writeEndTransfer(const EndTransferEvent&) = 0;
};

class Xtf23WriterDialect final : public XtfWriterDialect { ... };
class Xtf24WriterDialect final : public XtfWriterDialect { ... };
```

Die Klassen sind intern. Gemeinsame Logik wird nur extrahiert, wenn sie tatsächlich identisch ist. Keine künstliche Vererbungshierarchie, die versionsspezifische Regeln verschleiert.

## 12.7 Namespace-Verwaltung XTF 2.4

```cpp
class NamespaceTable final {
public:
    void bind(std::string prefix, std::string namespaceUri);
    std::string prefixFor(std::string_view namespaceUri) const;
    std::optional<std::string> namespaceFor(
        std::string_view prefix) const;
    std::string assignModelPrefix(std::string_view modelName,
                                  std::string_view namespaceUri);
};
```

Kollisionen, reservierte Präfixe und doppelte Namespace-Bindungen müssen getestet werden.

---

# 13. Geometrieabbildung

Der Kern arbeitet ausschliesslich mit IOM-Strukturen. Es gibt in diesem Repository keine GEOS- oder eigene numerische Geometrieklasse.

## 13.1 Stabile interne Tags

Die Implementierung definiert zentrale Konstanten beziehungsweise Factory-Methoden für:

```text
COORD
ARC
POLYLINE
SEGMENTS
MULTICOORD
MULTIPOLYLINE
SURFACE
AREA
MULTISURFACE
MULTIAREA
```

Die tatsächlichen IOM-Tagstrings müssen soweit möglich den etablierten `iox-ili`-Konventionen entsprechen. Alle Mapping-Entscheidungen werden in `docs/conformance.md` tabellarisch festgehalten.

## 13.2 Coordinate

Empfohlene IOM-Struktur:

```text
COORD
  C1 = primitive
  C2 = primitive, optional gemäss Kontext
  C3 = primitive, optional
```

Werte bleiben lexikalische Strings. Keine Umwandlung zu `double` im Kern.

## 13.3 Arc

```text
ARC
  C1
  C2
  C3 optional
  A1
  A2
  A3 optional
  R optional
```

Die genaue XTF-2.3-/2.4-Codierung muss gegen die Spezifikation und `iox-ili` verifiziert werden. Nicht in jeder Version vorhandene Felder werden versionsspezifisch behandelt.

## 13.4 Polyline

```text
POLYLINE
  lineattr optional -> IomObject
  sequence[1..n] -> SEGMENTS
    segment[1..n] -> COORD | ARC | custom line form
```

- `INCOMPLETE` beziehungsweise Clipping wird über `Consistency::Incomplete` und die versionsspezifische XML-Struktur abgebildet.
- Ungeclippte Polyline darf nur eine Sequenz besitzen, sofern die Spezifikation dies verlangt.
- Benutzerdefinierte Linienformen werden als normale strukturierte `IomObject`-Segmente bewahrt.
- Unbekannte Segmente dürfen nicht zu COORD degradiert werden.

## 13.5 Surface und Area

Flächen werden als strukturierte IOM-Geometrie mit äusseren und inneren Begrenzungen dargestellt. Der genaue kanonische Objektbaum muss in Phase 4 für XTF 2.3 und in Phase 6 für XTF 2.4 mit Golden Tests festgeschrieben werden.

Verbindlich:

- Exterior Ring;
- Interior Rings;
- Polyline-/Segmentstruktur;
- Clipping und Incomplete;
- AREA und SURFACE unterscheidbar;
- keine Polygonisierung oder Topologieberechnung im Kern.

## 13.6 Multi-Geometrien XTF 2.4

Die XTF-2.4-Multigeometriefamilie wird vollständig behandelt:

- `MULTICOORD`;
- `MULTIPOLYLINE`;
- `MULTISURFACE`;
- `MULTIAREA`.

Auch wenn einzelne Typen in den ursprünglichen Mindestanforderungen nicht explizit aufgezählt waren, ist eine halbe Multi-Geometrieunterstützung nicht akzeptabel. Die Implementierung muss die in der normativen XTF-2.4-Spezifikation vorhandene Familie konsistent abdecken.

## 13.7 Geometrie-Hilfsklassen

Intern:

```cpp
class GeometryReader final {
public:
    IomObject read23Geometry(...);
    IomObject read24Geometry(...);
};

class GeometryWriter final {
public:
    void write23Geometry(const IomObject&, XmlWriter&);
    void write24Geometry(const IomObject&, XmlWriter&,
                         NamespaceTable&);
};

class GeometryShapeValidator final {
public:
    std::vector<Diagnostic> validate(
        const IomObject& geometry,
        XtfVersion version,
        Strictness strictness) const;
};
```

Die Methoden dürfen intern feiner aufgeteilt werden. Reader und Writer müssen dieselben kanonischen IOM-Strukturen verwenden.

---

# 14. Direkte Integration mit `ilic-core`

## 14.1 Scope

`iox-ilic` wird erst implementiert, nachdem der generische XTF-2.3-/2.4-Reader/Writer stabil ist. Es linkt direkt:

```cmake
target_link_libraries(iox-ilic
    PUBLIC iox-xtf ilic-core
)
```

Es gibt kein abstraktes Modellprovider-API.

## 14.2 `IlicModelIndex`

```cpp
class IlicModelIndex final {
public:
    explicit IlicModelIndex(
        const ilic::metamodel::TransferDescription& model);

    const ilic::metamodel::Topic* findTopic(
        std::string_view scopedName) const;

    const ilic::metamodel::Viewable* findClass(
        std::string_view scopedName) const;

    const ilic::metamodel::Element* findProperty(
        const ilic::metamodel::Viewable& owner,
        std::string_view propertyName) const;

    std::vector<const ilic::metamodel::Element*> transferProperties(
        const ilic::metamodel::Viewable& owner) const;

    std::optional<XmlQualifiedName> xmlNameForClass(
        const ilic::metamodel::Viewable& viewable,
        XtfVersion version) const;

    std::optional<XmlQualifiedName> xmlNameForProperty(
        const ilic::metamodel::Viewable& owner,
        const ilic::metamodel::Element& property,
        XtfVersion version) const;
};
```

Die exakten `ilic`-Typnamen müssen beim Implementieren gegen den tatsächlichen `ilic-fork` geprüft werden. Der Agent darf keine Typnamen erfinden. Bei Abweichungen werden die Signaturen angepasst, die fachliche Verantwortung bleibt jedoch identisch.

## 14.3 Modellbewusster Reader

```cpp
struct IlicXtfReaderOptions final {
    XtfReaderOptions xtf;
    bool rejectUnknownTopics = false;
    bool rejectUnknownClasses = false;
    bool rejectUnknownProperties = false;
};

class IlicXtfReader final : public Reader {
public:
    IlicXtfReader(
        const ilic::metamodel::TransferDescription& model,
        IlicXtfReaderOptions options = {});

    ReadOutcome next() override;
    void feed(ByteView) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;
};
```

Umsetzung als Komposition um einen generischen `XtfReader`, nicht als Vererbung von `XtfReader`.

Aufgaben:

- aufgelöste fachliche Namen ergänzen;
- Topic/Class/Property prüfen;
- XTF-2.4-Namespaceabbildung präzisieren;
- Modellreihenfolge für Writer-Metadaten bereitstellen;
- keine vollständige Datenvalidierung vortäuschen.

## 14.4 Modellbewusster Writer

```cpp
struct IlicXtfWriterOptions final {
    XtfWriterOptions xtf;
    bool enforceTransferOrder = true;
    bool rejectUnknownClasses = true;
    bool rejectUnknownProperties = true;
};

class IlicXtfWriter final : public Writer {
public:
    IlicXtfWriter(
        const ilic::metamodel::TransferDescription& model,
        std::shared_ptr<OutputSink> output,
        IlicXtfWriterOptions options = {});

    void write(const IoxEvent&) override;
    void flush() override;
    void close() override;
    bool isClosed() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;
};
```

Der Writer darf bei `enforceTransferOrder=true` Attribute anhand des konkreten `ilic`-Metamodells in normative Transferreihenfolge bringen. Er muss dabei die ursprüngliche Reihenfolge nicht zerstören; er arbeitet auf einer geordneten View für die Ausgabe.

## 14.5 WASM mit und ohne `ilic`

Mindestens das modellfreie `iox-wasm` ist Pflicht. Optionales späteres Bundle:

```text
@interlis/iox-wasm          modellfrei
@interlis/iox-wasm/ilic     nur falls technisch und grössenmässig sinnvoll
```

Vor einer zweiten Variante muss die Bundlegrösse gemessen und dokumentiert werden. Die initiale Fertigstellung darf nicht von der `ilic`-WASM-Integration blockiert werden.

---

# 15. Stabile C-ABI

## 15.1 ABI-Grundsätze

- C99-kompatibler öffentlicher Header.
- Keine C++-Typen im Header.
- Opaque Handles.
- Jeder Fehler wird als Statuscode oder Result Handle zurückgegeben.
- Keine Exception über die Grenze.
- Caller-owned Input Buffer muss nach Funktionsrückkehr freigegeben werden können.
- Strings sind UTF-8.
- Result-Strings bleiben gültig bis zur Zerstörung des Result Handles.
- ABI-Version ist eine positive Ganzzahl.

## 15.2 Header

```c
typedef struct iox_reader iox_reader_t;
typedef struct iox_writer iox_writer_t;
typedef struct iox_result iox_result_t;

typedef enum iox_status {
    IOX_STATUS_OK = 0,
    IOX_STATUS_EVENT = 1,
    IOX_STATUS_NEED_INPUT = 2,
    IOX_STATUS_END = 3,
    IOX_STATUS_ERROR = -1,
    IOX_STATUS_INVALID_ARGUMENT = -2,
    IOX_STATUS_INVALID_STATE = -3
} iox_status_t;

uint32_t iox_abi_version(void);
const char* iox_version(void);

void* iox_alloc(size_t size);
void iox_free(void* ptr);

iox_reader_t* iox_reader_create(const char* format,
                                const char* options_json);
void iox_reader_destroy(iox_reader_t* reader);

iox_status_t iox_reader_feed(iox_reader_t* reader,
                             const uint8_t* data,
                             size_t size);
iox_status_t iox_reader_finish(iox_reader_t* reader);
iox_status_t iox_reader_next(iox_reader_t* reader,
                             iox_result_t** result);

iox_writer_t* iox_writer_create(const char* format,
                                const char* options_json);
void iox_writer_destroy(iox_writer_t* writer);

iox_status_t iox_writer_write_event_json(
    iox_writer_t* writer,
    const char* event_json,
    size_t event_json_size,
    iox_result_t** result);

iox_status_t iox_writer_finish(iox_writer_t* writer,
                               iox_result_t** result);
iox_status_t iox_writer_take_output(iox_writer_t* writer,
                                    iox_result_t** result);

const char* iox_result_json(const iox_result_t* result);
const uint8_t* iox_result_bytes(const iox_result_t* result);
size_t iox_result_size(const iox_result_t* result);
void iox_result_destroy(iox_result_t* result);
```

Die endgültige API darf erweitert oder leicht angepasst werden, wenn dies mit Tests und `docs/event-json-schema.md` begründet wird. Die Grundform mit Reader Handle, `feed`, `finish`, `next_event`, Writer Handle und Result Handle ist verbindlich.

## 15.3 Result JSON

Eventresultat:

```json
{
  "ok": true,
  "status": "event",
  "event": { ... },
  "diagnostics": []
}
```

Fehlerresultat:

```json
{
  "ok": false,
  "status": "error",
  "error": {
    "code": "xml.malformed",
    "message": "...",
    "location": {
      "sourceName": "data.xtf",
      "byteOffset": 123,
      "line": 5,
      "column": 19
    }
  },
  "diagnostics": []
}
```

## 15.4 ABI-Tests

- C-Compiler-Test, der nur `iox/abi/iox.h` inkludiert.
- Lebensdauer aller Handles.
- Nullpointer und ungültige Argumente.
- Mehrfaches `destroy(NULL)` ist zulässig oder klar dokumentiert.
- Keine Memory Leaks unter ASan.
- Event JSON entspricht exakt dem dokumentierten Schema.
- Native ABI und WASM ABI liefern dieselbe normalisierte JSON-Struktur.

---

# 16. JavaScript-/WASM-API

## 16.1 Paket

`packages/iox-wasm/package.json`:

```json
{
  "name": "@interlis/iox-wasm",
  "version": "0.1.0",
  "type": "module",
  "engines": {
    "node": ">=18"
  },
  "scripts": {
    "test": "node --test test/*.test.mjs"
  }
}
```

Publikation ist nicht Bestandteil des Auftrags. Der Scope wird lokal verwendet, ohne Verfügbarkeitsaussage für npm.

## 16.2 Modulinitialisierung

```ts
export interface IoxModuleOptions {
  locateFile?: (path: string) => string;
}

export function createIoxModule(
  options?: IoxModuleOptions
): Promise<IoxModule>;
```

Die Emscripten-Modulinitialisierung ist asynchron. Reader- und Writeroperationen sind danach synchron, solange der Benutzer keine Web Streams verwendet.

## 16.3 Reader

```ts
export interface XtfReaderOptions {
  strict?: boolean;
  sourceName?: string;
  expectedVersion?: "2.3" | "2.4";
  preserveUnknownExtensions?: boolean;
}

export class XtfReader implements Iterable<IoxEvent> {
  constructor(module: IoxModule,
              input: Uint8Array | ArrayBuffer | string,
              options?: XtfReaderOptions);

  [Symbol.iterator](): Iterator<IoxEvent>;
  readAll(): IoxEvent[];
  diagnostics(): Diagnostic[];
  close(): void;
}
```

Stringinput wird als UTF-8 encodiert. DOM-APIs sind verboten; `TextEncoder`/`TextDecoder` müssen in den Zielumgebungen verwendet oder sauber polyfilled werden.

## 16.4 Inkrementeller Reader

Spätere Phase:

```ts
export class IncrementalXtfReader {
  constructor(module: IoxModule,
              options?: XtfReaderOptions);

  feed(chunk: Uint8Array): IoxEvent[];
  finish(): IoxEvent[];
  diagnostics(): Diagnostic[];
  close(): void;
}
```

Diese Klasse ist die Grundlage für spätere Web-Streams-Unterstützung, ohne dass der C++-Kern geändert werden muss.

## 16.5 Writer

```ts
export interface XtfWriterOptions {
  version: "2.3" | "2.4";
  strict?: boolean;
  pretty?: boolean;
  sender?: string;
}

export class XtfWriter {
  constructor(module: IoxModule,
              options: XtfWriterOptions);

  write(event: IoxEvent): void;
  finish(): Uint8Array;
  diagnostics(): Diagnostic[];
  close(): void;
}
```

## 16.6 Convenience-Funktionen

```ts
export function readAll(
  module: IoxModule,
  input: Uint8Array | ArrayBuffer | string,
  options?: XtfReaderOptions
): IoxEvent[];

export function writeAll(
  module: IoxModule,
  events: Iterable<IoxEvent>,
  options: XtfWriterOptions
): Uint8Array;
```

## 16.7 Worker

`worker.js` stellt ein kleines Message-Protokoll bereit:

```text
init
readAll
writeAll
close
```

Worker-Antworten besitzen Request IDs und strukturierte Fehler. `ArrayBuffer` soll wenn möglich als Transferable zurückgegeben werden.

## 16.8 TypeScript-Deklarationen

`index.d.ts` muss vollständig sein. Kein `any` für öffentliche Events. Discriminated Unions:

```ts
export type IoxEvent =
  | StartTransferEvent
  | StartBasketEvent
  | ObjectEvent
  | EndBasketEvent
  | EndTransferEvent;
```

`event` ist der Discriminator.

---

# 17. Teststrategie und Qualitätsanforderungen

## 17.1 Grundsatz

Tests sind keine abschliessende Phase, sondern Bestandteil jeder Implementierungsphase. Keine Phase darf committen, bevor alle in ihrem Scope liegenden Tests erfolgreich laufen.

Reguläre Tests dürfen benötigen:

- CMake;
- C++ Compiler;
- CTest;
- Node.js ab 18 für WASM-Tests;
- Emscripten 3.1.64 für WASM-Builds.

Reguläre Tests dürfen **nicht** benötigen:

- Java;
- Netzwerkzugriff;
- laufende Server;
- Docker;
- installierte INTERLIS-Tools;
- Systempakete für Expat.

## 17.2 Testframework

Analog zum Stil von `ilic-fork` ist kein schweres Testframework erforderlich. Es soll ein kleiner gemeinsamer Test-Support entstehen:

```cpp
namespace iox::test {

void fail(const char* file,
          int line,
          std::string message);

#define IOX_CHECK(expr) ...
#define IOX_CHECK_EQ(expected, actual) ...
#define IOX_CHECK_THROWS_CODE(expr, code) ...

class TempDirectory final { ... };
class Fixture final { ... };

}
```

Jede Test-Executable gibt bei Erfolg 0 und bei Fehlern ungleich 0 zurück. Einzelne Testgruppen werden als separate CTest-Tests registriert, damit Fehler gut lokalisierbar bleiben.

## 17.3 Testebenen

### Unit Tests

- `ByteView`;
- IOM COW-Verhalten;
- Attribut- und Wertreihenfolge;
- Eventvarianten;
- Eventzustandsautomaten;
- Diagnostics;
- Name Mapping;
- Namespace Table;
- XML Escaping;
- JSON-Schema;
- Geometry Shape Validator;
- Format Registry.

### Component Tests

- Expat Chunk Parser;
- XTF-2.3 Dialekt;
- XTF-2.4 Dialekt;
- Writerdialekte;
- C-ABI;
- `iox-ilic`-Namensauflösung.

### Integration Tests

- Datei → Events;
- Events → Datei;
- Datei → Events → Datei → Events;
- Native → JSON → WASM;
- WASM → JSON → Native;
- ReaderFactory/WriterFactory;
- `iox-dump`.

### Conformance Tests

- offizielle XTF-Testfälle;
- kontrollierte Ausschnitte aus `iox-ili`-Tests;
- Golden Fixtures;
- dokumentierte Differenzen.

### Fuzz Tests

- beliebige XML-Bytes;
- gültige XML-Dokumente mit zufälligen Chunk-Grenzen;
- Event JSON;
- IOM-Objektoperationen;
- Writer Event Sequenzen;
- Roundtrip Fuzzing mit kleinen generierten IOM-Bäumen.

## 17.4 Fixture-Struktur

```text
test/fixtures/
├── shared/
│   ├── unicode/
│   ├── invalid-xml/
│   └── event-json/
├── xtf23/
│   ├── minimal/
│   ├── header/
│   ├── primitive/
│   ├── structures/
│   ├── references/
│   ├── baskets/
│   ├── geometry/
│   ├── incremental/
│   ├── invalid/
│   └── extensions/
├── xtf24/
│   ├── minimal/
│   ├── namespaces/
│   ├── primitive/
│   ├── structures/
│   ├── references/
│   ├── baskets/
│   ├── geometry/
│   ├── multigeometry/
│   ├── incremental/
│   ├── invalid/
│   └── extensions/
└── ilic/
    ├── models/
    ├── xtf23/
    └── xtf24/
```

Jeder Fixture-Ordner enthält optional:

```text
input.xtf
expected-events.ndjson
expected-output.xtf
expected-diagnostics.json
README.md
```

## 17.5 Semantischer Eventvergleich

```cpp
struct EventComparisonOptions final {
    bool ignoreSourceLocations = true;
    bool ignoreXmlPrefixHints = true;
    bool compareDiagnostics = false;
};

class EventComparator final {
public:
    ComparisonResult compare(
        const std::vector<IoxEvent>& left,
        const std::vector<IoxEvent>& right,
        EventComparisonOptions options = {}) const;
};
```

Roundtrip-Tests vergleichen normalisierte Events, nicht rohe XML-Bytes.

## 17.6 Chunk-Grenzentests

Für jedes kleine wichtige XTF-Fixture:

1. gesamte Datei als ein Chunk;
2. ein Byte pro Chunk;
3. Chunkgrössen 2, 3, 7, 64, 4096;
4. deterministische Pseudozufallsgrenzen;
5. Grenzen mitten in UTF-8-Mehrbytezeichen;
6. Grenzen mitten in XML-Entities;
7. Grenzen zwischen Starttag, Attributen und Endtag.

Alle Varianten müssen dieselbe Eventfolge erzeugen.

## 17.7 Negativtests

Mindestens:

- leere Eingabe;
- abgeschnittenes XML;
- falscher Root;
- falscher Namespace;
- XTF 2.2 als unsupported;
- DTD;
- externe Entity;
- doppelte oder fehlende Headerteile;
- fehlendes BID;
- unerwartetes TID;
- ungültige Eventreihenfolge;
- unbekannte Consistency/Operation/Kind;
- ungültige Referenzattribute;
- ungültige Geometriestruktur;
- Ressourcenlimitüberschreitung;
- Writer ohne StartTransfer;
- Writer mit Objekt ausserhalb Basket;
- doppeltes EndTransfer;
- Feed nach Finish;
- Finish zweimal;
- ABI-Nullpointer;
- ungültiges JSON an der ABI.

## 17.8 Unicode-Tests

Mindestens:

- deutsche Umlaute;
- französische Akzente;
- rätoromanische Zeichen;
- griechische und kyrillische Schrift;
- CJK;
- Emoji in TEXT-Feldern, soweit XML erlaubt;
- kombinierende Zeichen;
- Zeichen über U+FFFF;
- ungültige UTF-8-Sequenzen;
- verbotene XML-Steuerzeichen;
- CR/LF-Normalisierung.

## 17.9 Coverage

Ziele:

- `iox-core`, `iox-json`, `iox-xml`, `iox-xtf`, `iox-abi`: mindestens 90 Prozent Line Coverage;
- mindestens 85 Prozent Branch Coverage;
- alle öffentlichen Methoden mindestens einmal auf Erfolgs- und Fehlerpfad;
- `iox-ilic` nach seiner Einführung ebenfalls mindestens 85/75, Ziel 90/85.

`scripts/coverage.sh`:

- erkennt Clang/LLVM oder GCC;
- baut in `build/coverage`;
- führt CTest aus;
- erzeugt HTML und Text Summary;
- prüft Grenzwerte;
- schliesst Drittanbieter, Tests und generierte WASM-Glue-Dateien aus;
- bricht bei Unterschreitung ab.

Coverage-Ziele gelten ab der Phase, in der das entsprechende Modul produktionsreif erklärt wird. Frühere Phasen müssen ihren eigenen Scope bereits hoch abdecken.

## 17.10 Sanitizer

Lokale Debug-Tests unter unterstützten Unix-Systemen:

```text
ASan
UBSan
```

Optional:

```text
LSan, soweit Plattform unterstützt
```

Keine pauschale Unterdrückungsdatei. Jede notwendige Unterdrückung muss eng begrenzt und dokumentiert sein.

## 17.11 Fuzzing

CMake Targets:

```text
iox_fuzz_xml_reader
iox_fuzz_xtf_reader
iox_fuzz_event_json
iox_fuzz_iom_object
iox_fuzz_writer_events
```

Fuzz Targets bauen nur bei `IOX_ENABLE_FUZZING=ON` und Clang/libFuzzer. Ein kleiner reproduzierbarer Corpus wird eingecheckt. Crashes werden als Regression Fixtures übernommen.

## 17.12 Java-Differentialtests

`scripts/differential-java.sh` ist optional und nicht Teil des normalen `ctest`.

Es darf:

- einen lokal vorhandenen `iox-ili`-Build verwenden;
- XTF-Fixtures über Java in Event JSON umwandeln;
- Native Events dagegen vergleichen;
- neue Golden Fixtures generieren.

Es darf nicht automatisch fremde Binärdateien herunterladen oder Netzwerkzugriff in normalen Tests voraussetzen. Generierte Goldens werden geprüft, nachvollziehbar dokumentiert und eingecheckt.

## 17.13 Native/WASM-Parität

Für jedes Paritätsfixture:

```text
native reader → canonical NDJSON
WASM reader   → canonical NDJSON
```

Die Dateien müssen identisch sein, nachdem ausschliesslich Source Locations und Prefix Hints gemäss dokumentierter Regel normalisiert wurden.

Dasselbe gilt für Writeroutput: Entweder Bytegleichheit bei identischen Optionen oder semantische Eventgleichheit nach erneutem Lesen. Bevorzugt wird Bytegleichheit, da beide denselben C++-Writer verwenden.

---

# 18. Performance- und Ressourcenanforderungen

Performance ist hinter Korrektheit und Konformität priorisiert, aber die Streaming-Architektur darf nicht durch vermeidbare Vollpufferung entwertet werden.

## 18.1 Reader

- Gesamtinput wird nicht vollständig gespeichert.
- Es wird höchstens der aktuelle XML-/IOM-Kontext, die begrenzte Eventqueue und noch nicht konsumierter Input gehalten.
- Ein normaler Basket kann objektweise gestreamt werden.
- `readBasket()` ist die einzige bewusst basketweise puffernde Convenience-Schicht.
- Primitive Textwerte werden möglichst einmal materialisiert.
- Reader muss grosse Dateien verarbeiten können, ohne linearen Speicherverbrauch zur Dateigrösse.

## 18.2 Writer

- Writer schreibt fortlaufend zum `OutputSink`.
- Keine vollständige Ausgabedatei im Kern puffern.
- WASM `VectorOutputSink` darf aus Convenience-Gründen puffern; die C-ABI muss zusätzlich chunkweises `take_output` erlauben.

## 18.3 Messbare Smoke-Benchmarks

Kein umfassendes Benchmarkframework erforderlich. Ein optionaler lokaler Smoke-Test soll dokumentieren:

- 100'000 einfache Objekte lesen;
- 100'000 einfache Objekte schreiben;
- maximale Queue bleibt begrenzt;
- keine aussergewöhnliche Speicherzunahme nach mehreren Wiederholungen.

Diese Benchmarks sind keine harten Release Gates, aber Regressionen müssen sichtbar sein.

---

# 19. Dokumentationsanforderungen

## 19.1 `README.md`

Muss enthalten:

- Zweck und Scope;
- XTF 2.3/2.4;
- Native/WASM;
- Status pro Phase;
- Buildkommandos;
- minimaler C++ Reader;
- minimaler C++ Writer;
- JavaScript-Beispiel;
- Erweiterungsbeispiel;
- klare Nicht-Ziele;
- Lizenz.

## 19.2 `docs/architecture.md`

Muss erklären:

- Module und Abhängigkeiten;
- warum `iox-ilic` separat, aber direkt gekoppelt ist;
- Eventstream;
- IOM COW;
- XML Streaming;
- XTF Dialekte;
- C-ABI;
- WASM;
- Factory/Registry;
- Error Model;
- Extension Policy.

Mermaid-Diagramme sind erlaubt, aber Text bleibt vollständig verständlich.

## 19.3 `docs/roadmap.md`

Enthält alle Phasen, Akzeptanzkriterien und Statuswerte:

```text
not-started
in-progress
blocked
completed
```

Keine Phase wird als completed markiert, bevor Commit und Tests existieren.

## 19.4 `docs/phase-status.md`

Maschinen- und menschenlesbare Tabelle:

```text
Phase | Status | Commit | Tests | Artefakt | Notes
```

Jede Phase aktualisiert genau ihre Zeile. Ein fehlgeschlagener Lauf dokumentiert den letzten erfolgreichen Commit und den präzisen Blocker.

## 19.5 `docs/conformance.md`

Enthält:

- normative Dokumentversionen;
- Quell-SHAs von `iox-ili`, `iox-api`, Test-Suite, historischem IOM und `ilic-fork`;
- XTF-2.3-/2.4-Featurematrix;
- IOM-Geometriemapping;
- bekannte Unterschiede zu Java;
- bewusste Nicht-Unterstützung;
- Fixture-Herkunft und Lizenz;
- Regeln zur Golden-Erzeugung.

## 19.6 `docs/extending-formats.md`

Vollständiges Beispiel eines weiteren Readers/Writers anhand von JSON Events:

- Descriptor;
- Registry;
- Reader;
- Writer;
- Tests;
- Fehlerbehandlung;
- kein globales Plugin-System.

## 19.7 `docs/wasm.md`

- Emscripten-Version;
- Build;
- ABI;
- JS Wrapper;
- Browser;
- Worker;
- Node;
- Speicher/Lifetime;
- inkrementelles Feed;
- Bundlevarianten;
- bekannte Einschränkungen.

---

# 20. `AGENTS.md` – verbindlicher Inhalt

Der Agent erstellt in Phase 0 eine Root-Datei `AGENTS.md`. Sie muss knapp genug für wiederholtes Lesen sein, aber folgende Inhalte verbindlich abdecken.

Empfohlener vollständiger Inhalt:

````markdown
# AGENTS.md

## Mission

Build `iox-cpp`: a C++17, streaming, WebAssembly-capable INTERLIS XTF 2.3/2.4 reader and writer framework. The event stream and generic IOM object model are the architectural core.

## Read first

Before changing code, read:

1. `docs/architecture.md`
2. `docs/roadmap.md`
3. `docs/conformance.md`
4. the relevant file under `.agents/skills/`
5. `docs/phase-status.md`

## Non-negotiable architecture

- `iox-core` is format- and model-independent.
- `iox-xtf` supports generic model-free XTF processing.
- `iox-ilic` is optional and links directly to `ilic-core`; do not create a generic model-provider framework.
- `IoxEvent` is a `std::variant` event stream.
- `IomObject` is a copy-on-write value-like handle backed by `std::shared_ptr`.
- Preserve attribute and repeated-value order.
- XTF 2.3 and 2.4 use separate internal dialect implementations.
- Expat is private to the XML implementation.
- No exceptions cross the C ABI.
- No DOM dependency in JavaScript.
- No ITF, validator, GEOS/GDAL integration, dynamic plugins, CI/CD, publishing, or network-dependent regular tests.

## Build stack

- CMake 3.20+
- C++17
- CTest
- Emscripten version from `.emscripten-version`
- Node.js 18+
- static pinned third-party dependencies via `FetchContent`

## Standard commands

```sh
./scripts/build-native.sh
./scripts/test-native.sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
./scripts/coverage.sh
```

Use CMake directly only when diagnosing a build issue. Keep scripts as the documented path.

## Phase execution

- Work only on the current phase in `docs/roadmap.md`.
- Start from a clean working tree.
- Mark the phase `in-progress` before implementation.
- Implement, document, build, and test the phase completely.
- Never disable or weaken a test to make a phase pass.
- Update `docs/phase-status.md` and `docs/conformance.md` when relevant.
- Commit each completed phase with `phase N: <short outcome>`.
- Begin the next phase automatically.
- Do not push.

## Failure policy

For a failing phase, perform at most three materially different repair attempts. After that:

- restore a clean working tree at the last successful phase commit;
- record the exact failure, commands, diagnostics, and attempted fixes in `docs/phase-status.md`;
- stop without asking a human question.

Never use destructive Git commands against pre-existing user work. Never rewrite completed phase commits.

## Quality gates

- All relevant CTest tests pass.
- WASM tests pass once introduced.
- Public API success and failure paths are tested.
- No regular test requires Java or network access.
- Core production targets meet the documented coverage thresholds before final completion.
- Sanitizer and fuzz targets must remain buildable where supported.

## Coding style

- RAII and explicit ownership.
- PImpl for public classes that would expose parser or dependency details.
- No raw owning pointers.
- No global mutable registration.
- No silent data loss.
- Deterministic output and diagnostics.
- Keep public headers dependency-light.
- Prefer small classes with one responsibility.
- Document invariants, not obvious syntax.

## Skills

Use the relevant skill file:

- `.agents/skills/architecture/SKILL.md`
- `.agents/skills/native-build/SKILL.md`
- `.agents/skills/wasm-build/SKILL.md`
- `.agents/skills/xtf-conformance/SKILL.md`
- `.agents/skills/testing/SKILL.md`
- `.agents/skills/phase-execution/SKILL.md`

These files are canonical project instructions even if the coding client does not auto-discover skills. Read them explicitly.
````

Der Agent darf Formulierungen verbessern, aber keine fachliche Regel entfernen.

---

# 21. Repo-lokale Skills

Die Skills müssen sowohl von ChatGPT/Codex-basierten Agents als auch von OpenCode nutzbar sein. Da automatische Discovery je Client variieren kann, gilt:

- kanonischer Ort ist `.agents/skills/<name>/SKILL.md`;
- Root `AGENTS.md` referenziert alle Skills;
- Startprompt verlangt ihr explizites Lesen;
- kein Symlink-Zwang, damit Windows funktioniert;
- jeder Skill besitzt YAML-Frontmatter mit `name` und `description`;
- jeder Skill ist eigenständig verständlich und verweist auf konkrete Projektdateien.

## 21.1 `architecture/SKILL.md`

````markdown
---
name: architecture
description: Protect the module boundaries, public API invariants, event model, IOM copy-on-write semantics, and direct optional ilic-core integration of iox-cpp.
---

# Architecture skill

Use this skill for public APIs, module boundaries, ownership, event flow, IOM changes, format registration, and ilic integration.

## Required reading

- `docs/architecture.md`
- `docs/conformance.md`
- public headers under `include/iox/`

## Invariants

1. `iox-core` has no XML, Expat, XTF, JSON, or ilic dependency.
2. `iox-xtf` has no `ilic-core` dependency.
3. `iox-ilic` links directly to concrete `ilic-core` types. Never introduce a generic provider hierarchy.
4. The normative API is the ordered `IoxEvent` stream.
5. `IomObject` preserves attribute order and repeated-value order.
6. Public C++ ownership uses RAII. No public retain/release API.
7. Public dependency-heavy classes use PImpl.
8. Unknown fachlich relevant content is preserved or diagnosed; never silently dropped.
9. XTF 2.3 and XTF 2.4 remain separate dialect implementations.
10. Convenience APIs delegate to the normative event API.

## Review checklist

- Does the change add a dependency in the wrong direction?
- Does it expose Expat or ilic internals in unrelated public headers?
- Does a mutation correctly detach shared IOM state?
- Is order preserved?
- Is output deterministic?
- Are fatal and nonfatal failures separated?
- Does the C ABI catch all exceptions?
- Can the generic model-free path still build and run?
- Are Native and WASM semantics identical?

Reject architecture shortcuts that make a single fixture pass while violating these invariants.
````

## 21.2 `native-build/SKILL.md`

````markdown
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
````

## 21.3 `wasm-build/SKILL.md`

````markdown
---
name: wasm-build
description: Build and test the pinned Emscripten WebAssembly module and its browser, worker, and Node.js wrappers without introducing DOM dependencies.
---

# WASM build skill

## Standard flow

```sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

## Invariants

- Emscripten version exactly matches `.emscripten-version`.
- Output is an ES module with `MODULARIZE=1` and `EXPORT_ES6=1`.
- Supported environments are `web,worker,node`.
- JavaScript does not depend on `window`, `document`, or other DOM-only globals.
- C++ exceptions are caught inside the C ABI.
- Input crosses the boundary as bytes or UTF-8, not raw JS object pointers.
- Event and diagnostic JSON schemas are stable and tested.
- Handles are always destroyed, including error paths.
- Large inputs use incremental feed in the final architecture.

## Tests

- Node.js `node --test` is mandatory.
- Browser and worker APIs need smoke tests that can run locally with a minimal harness.
- Compare normalized Native and WASM event JSON.
- Test repeated create/read/destroy cycles for leaks and stale state.

Do not change the ABI only in JavaScript. Update C header, implementation, schema docs, typings, and tests together.
````

## 21.4 `xtf-conformance/SKILL.md`

````markdown
---
name: xtf-conformance
description: Implement and verify XTF 2.3/2.4 transfer semantics against the normative specifications, iox-ili behavior, and checked-in golden fixtures.
---

# XTF conformance skill

## Source priority

1. Current normative INTERLIS reference manual for the relevant version.
2. `iox-api`/`iox-ili` behavior.
3. Official XTF test suite.
4. Historical IOM behavior as supporting evidence only.

## Workflow for every feature

1. Identify the normative rule and record the reference in `docs/conformance.md`.
2. Inspect the corresponding Java reader/writer behavior.
3. Add minimal positive and negative fixtures.
4. Implement the version-specific dialect behavior.
5. Add chunk-boundary tests.
6. Add semantic roundtrip tests.
7. Add Native/WASM parity once WASM is available.
8. Document deliberate differences.

## Rules

- Never use lexical XML comparison as the only correctness check.
- Never infer missing namespace information silently.
- Preserve unknown fachlich relevant elements or diagnose them.
- Treat XML malformed input as fatal.
- Keep XTF 2.3 and 2.4 logic separate where encoding differs.
- Geometry uses canonical IOM structures; no GEOS conversion.
- No regular test may call Java or the network.
````

## 21.5 `testing/SKILL.md`

````markdown
---
name: testing
description: Design and run unit, component, roundtrip, conformance, Unicode, coverage, sanitizer, fuzz, and Native/WASM parity tests for iox-cpp.
---

# Testing skill

## Mandatory test dimensions

- success and failure paths
- event order
- IOM COW and ordering
- malformed XML
- strict versus lenient behavior
- one-byte and randomized chunk boundaries
- Unicode and invalid UTF-8
- references and structures
- all required geometry families
- semantic roundtrip
- deterministic writer bytes
- C ABI argument/lifetime errors
- Native/WASM parity

## Fixture rules

- Keep fixtures minimal and named after one behavior.
- Record provenance and license.
- Golden files require a documented generation method.
- Never overwrite a golden merely because a test fails; determine whether implementation or golden is wrong.

## Coverage

Use `./scripts/coverage.sh`. Exclude third-party and generated glue only. Do not exclude difficult project code to reach thresholds.

## Fuzzing

Every fuzz crash becomes a deterministic regression test before the fix is considered complete.

Never delete, skip, or weaken a valid test to complete a phase.
````

## 21.6 `phase-execution/SKILL.md`

````markdown
---
name: phase-execution
description: Execute the autonomous implementation roadmap one tested, useful, committed phase at a time with deterministic failure handling.
---

# Phase execution skill

## Before a phase

1. Read `AGENTS.md`, this skill, and the phase in `docs/roadmap.md`.
2. Verify repository identity and current branch.
3. Run `git status --short`.
4. Do not destroy pre-existing user changes.
5. Verify all previous phase tests pass.
6. Mark only the current phase `in-progress`.

## During a phase

- Implement only the stated scope plus necessary fixes.
- Keep the artifact usable at phase end.
- Add tests before or with implementation.
- Update documentation as architecture becomes concrete.
- Run focused tests frequently and full phase gates before commit.

## Completion

1. Run all required native tests.
2. Run WASM tests once introduced.
3. Run coverage/sanitizer gates required by the phase.
4. Update `docs/phase-status.md` with exact commands and result.
5. Mark the phase `completed`.
6. Commit with `phase N: <outcome>`.
7. Confirm a clean working tree.
8. Start the next phase automatically.

## Failure

Attempt at most three materially different repairs. Do not count rerunning the same command as a repair. After three unsuccessful approaches:

- save useful diagnostics in `docs/phase-status.md`;
- return the tree to the last successful phase commit without rewriting it;
- keep user-owned pre-existing work intact;
- stop and report the blocker without asking a question.

Do not push, publish, or create CI/CD files.
````

---

# 22. Phasenplan für die autonome Umsetzung

Jede Phase muss:

- auf einem sauberen Working Tree beginnen;
- alle Tests vorheriger Phasen weiterhin bestehen lassen;
- ein praktisch nutzbares Artefakt liefern;
- Dokumentation aktualisieren;
- mit einem eigenen Commit enden;
- automatisch in die nächste Phase übergehen.

Der Agent darf innerhalb einer Phase intern kleinere Schritte bilden. Er darf Phasen nicht zusammenlegen, wenn dadurch ein definierter Zwischenartefakt oder Qualitätsgate entfällt.

## Phase 0 – Repository-Baseline, Build und Agentensteuerung

### Ziel

Ein reproduzierbares C++17-/CMake-/CTest-/Emscripten-Grundgerüst, das nativ und als minimales WASM-Modul baut. Noch keine fachliche XTF-Funktion.

### Zu erstellen

- `CMakeLists.txt`;
- CMake-Hilfsdateien;
- `.emscripten-version` mit `3.1.64`;
- `LICENSE` MIT;
- `THIRD_PARTY_NOTICES.md`;
- `.gitignore`;
- `AGENTS.md`;
- alle sechs Skills;
- `docs/architecture.md` initial;
- `docs/roadmap.md` mit allen Phasen;
- `docs/conformance.md` mit gepinnten Referenzen;
- `docs/phase-status.md`;
- Build- und Testskripte;
- `iox-core` mit `Version.h/.cpp`;
- minimales `iox-abi` mit `iox_abi_version()` und `iox_version()`;
- minimales `iox-wasm` ES-Modul;
- Native-, C-Header- und Node-WASM-Smoke-Test.

### Verifikation

```sh
./scripts/build-native.sh
./scripts/test-native.sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

### Nutzbares Artefakt

- statische `iox-core`-/`iox-abi`-Bibliothek;
- ladbares WASM-Modul mit Versionsabfrage;
- funktionsfähige autonome Agentensteuerung.

### Commit

```text
phase 0: establish native wasm and agent baseline
```

---

## Phase 1 – IOM-Kern, Events und JSON-Eventformat

### Ziel

Ein formatunabhängiges, produktiv verwendbares Event- und Objektmodell inklusive JSON Reader/Writer und erster C-ABI/WASM-Nutzung.

### Implementierung

- Diagnostics und `IoxError`;
- `ByteView`;
- `IomName` und `XmlQualifiedName`;
- `IomValue`;
- `IomObject` mit COW;
- Eventtypen und `IoxEvent`;
- Reader-/Writerinterfaces;
- `OutputSink`-Implementierungen;
- Format Registry und Factories;
- `JsonEventReader`/`JsonEventWriter`;
- Event JSON Schema;
- C-ABI für JSON Event Reader/Writer;
- JS Wrapper für `json-events`;
- Beispiel Custom Format beziehungsweise Registry.

### Methodengates

- alle in Abschnitt 7 definierten Kernmethoden;
- COW success/failure tests;
- geordnete Attribute/Werte;
- Referenzmetadaten;
- zyklusgeschützte JSON-Serialisierung;
- Event State Machine für Writer;
- Registry ohne globale Initialisierungsreihenfolge.

### Tests

- Unit Tests sämtlicher öffentlicher Kernmethoden;
- NDJSON in 1-Byte-Chunks;
- ungültiges JSON;
- Eventreihenfolge;
- Native/WASM identisches JSON;
- ABI Lifetime und Nullpointer;
- Unicode.

### Nutzbares Artefakt

Ein allgemeines, WASM-fähiges Iox Event Framework, das Events und IOM-Objekte als NDJSON lesen und schreiben kann. Dies beweist bereits die spätere Erweiterbarkeit um weitere Formate.

### Commit

```text
phase 1: add iom events registry and json streaming
```

---

## Phase 2 – Sichere inkrementelle XML-Schicht und XTF-Erkennung

### Ziel

Ein sicherer, chunkfähiger Expat-Parser sowie ein deterministischer XML-Writer. `XtfReader` kann Version und Header erkennen und als `StartTransferEvent` ausgeben; vollständige Datenobjekte folgen später.

### Implementierung

- Expat gepinnt mit FetchContent;
- `ExpatParser`;
- XML Name Codec;
- XML Limits;
- `XmlWriter`;
- XTF Versionsdetektor;
- Grundgerüst `XtfReader` mit PImpl;
- Grundgerüst `XtfWriter` und State Machine;
- Parsing/Schreiben minimaler XTF-2.3-/2.4-Header;
- ReaderFactory/WriterFactory Format `xtf`;
- `docs/conformance.md` Header-Matrix.

### Tests

- DTD/externe Entity abgelehnt;
- UTF-8 und XML-Zeichen;
- XML-Escaping;
- Namespace Scopes;
- ein Byte pro Chunk;
- abgeschnittenes XML;
- XTF 2.2 abgelehnt;
- minimaler 2.3-Header;
- minimaler 2.4-Header;
- deterministic XML Writer;
- Native/WASM Header-Parität.

### Nutzbares Artefakt

Ein Native/WASM-XTF-Inspector, der valide XTF-2.3-/2.4-Dateien erkennt, Headerinformationen streamend extrahiert und minimale leere Transfers schreiben kann.

### Commit

```text
phase 2: add secure xml streaming and xtf headers
```

---

## Phase 3 – XTF 2.3 Basisobjekte und Referenzen

### Ziel

XTF 2.3 ohne Geometrie vollständig für Header, Baskets, normale Objekte, primitive Werte, Strukturen, Mehrfachwerte und Referenzen lesen und schreiben.

### Implementierung

- `Xtf23Dialect`;
- `Xtf23WriterDialect`;
- Basketmetadaten;
- Operation/Consistency/Kind/State/Domains;
- Objektstack und Attributbuilder;
- primitive Attribute;
- rekursive Strukturen;
- LIST/BAG-ähnliche Mehrfachwerte entsprechend Transfercodierung;
- Referenzen mit REF/BID/ORDER_POS;
- Delete;
- unbekannte fachliche Objektattribute;
- model-free Writer Naming;
- Strict/Lenient-Grundregeln.

### Tests

- jedes Feature positiv und negativ;
- Reihenfolge;
- fehlende BID/TID;
- Referenzvarianten;
- unbekannte Elemente erhalten;
- XTF 2.3 semantic roundtrip;
- Chunk-Matrix;
- Native/WASM-Parität.

### Nutzbares Artefakt

Ein praktisch verwendbarer XTF-2.3-Reader/Writer für alle nichtgeometrischen Datenmodelle.

### Commit

```text
phase 3: implement xtf 2.3 objects and references
```

---

## Phase 4 – XTF 2.3 Geometrie und Produktionshärtung

### Ziel

Vollständiger XTF-2.3-Scope inklusive Geometrie und Conformance Fixtures.

### Implementierung

- COORD;
- ARC;
- POLYLINE;
- SEGMENTS;
- benutzerdefinierte Linienformen;
- SURFACE;
- AREA;
- Clipping/INCOMPLETE;
- Geometrie Shape Validator;
- vollständiger 2.3 Header/OID Space/Alias Scope;
- offizielle Test-Suite-Fixtures, soweit lizenzrechtlich und fachlich geeignet;
- optionaler Java-Differentialgenerator.

### Tests

- jede Geometriestruktur;
- mehrere Segmente;
- mehrere Sequenzen bei Clipping;
- innere/äussere Begrenzungen;
- ungültige Struktur;
- Roundtrip;
- 1-Byte-Chunks;
- Fuzz Target für XTF Reader aktivierbar;
- 90/85 Coverage für produktionsreifen 2.3 Scope.

### Nutzbares Artefakt

Produktionsreifer modellfreier XTF-2.3-Reader/Writer im vereinbarten Scope.

### Commit

```text
phase 4: complete and harden xtf 2.3 geometry
```

---

## Phase 5 – XTF 2.4 Basisobjekte, Namespaces und Referenzen

### Ziel

XTF 2.4 ohne Geometrie vollständig für Header, Namespaces, Baskets, normale Objekte, Strukturen und Referenzen lesen und schreiben.

### Implementierung

- `Xtf24Dialect`;
- `Xtf24WriterDialect`;
- INTERLIS-/Geometry-/XSI-Namespacegrundlagen;
- Modellnamespace-Tabelle;
- `IomName` mit originalem XML-QName;
- Namespace-Präfixzuweisung;
- Baskets und Objects;
- primitive/strukturierte Werte;
- Referenzen;
- Delete;
- Kind/State/Consistency/Domains;
- model-free roundtrip bei gespeicherten XML-Namen;
- klare Fehler bei nicht ableitbaren Namen.

### Tests

- Namespace-Kollisionen;
- mehrere Modelle;
- Prefixänderung bei semantischer Gleichheit;
- unbekannter Namespace;
- manuell erzeugtes Objekt mit/ohne ausreichende Namensmetadaten;
- Roundtrip;
- Chunk-Matrix;
- Native/WASM-Parität.

### Nutzbares Artefakt

Ein praktisch verwendbarer modellfreier XTF-2.4-Reader/Writer für alle nichtgeometrischen Datenmodelle.

### Commit

```text
phase 5: implement xtf 2.4 namespaces objects and references
```

---

## Phase 6 – XTF 2.4 Geometrie und Produktionshärtung

### Ziel

Vollständiger XTF-2.4-Scope inklusive Geometrie und Multi-Geometrien.

### Implementierung

- COORD;
- ARC;
- POLYLINE;
- MULTICOORD;
- MULTIPOLYLINE;
- SURFACE;
- AREA;
- MULTISURFACE;
- MULTIAREA;
- Exterior/Interior;
- benutzerdefinierte Linienformen, soweit XTF 2.4 erlaubt;
- Clipping/INCOMPLETE;
- vollständige 2.4 Featurematrix.

### Tests

- jede Geometrieklasse;
- leere/einzelne/mehrfache Member entsprechend Spezifikation;
- verschachtelte Rings;
- invalid structures;
- Roundtrip;
- Chunk-Matrix;
- Fuzzing;
- 90/85 Coverage für produktionsreifen 2.4 Scope.

### Nutzbares Artefakt

Produktionsreifer modellfreier XTF-2.4-Reader/Writer im vereinbarten Scope.

### Commit

```text
phase 6: complete and harden xtf 2.4 geometry
```

---

## Phase 7 – Vollständige C-ABI und inkrementelles WASM

### Ziel

Die bereits vorhandene ABI wird auf vollständiges XTF Feed/Next/Writer-Streaming erweitert.

### Implementierung

- Reader Handle für `xtf`;
- `feed`, `finish`, `next`;
- Writer Handle;
- Event JSON → Writer;
- inkrementelles `take_output`;
- alle Exceptions abgefangen;
- stabile Statuscodes;
- ABI Version 1;
- Memory Lifecycle Tests;
- wiederholte Sessions.

### Tests

- C-only Consumer;
- kleine und grosse Chunks;
- NeedInput/Event/End Sequenz;
- Writeroutput stückweise;
- Fehlerresultate;
- ASan Native ABI;
- Node WASM Low-Level ABI;
- Native/WASM Parität.

### Nutzbares Artefakt

Stabile native und WebAssembly-C-ABI für XTF 2.3/2.4 Reader und Writer.

### Commit

```text
phase 7: finalize streaming c abi and wasm core
```

---

## Phase 8 – Idiomatisches `@interlis/iox-wasm`

### Ziel

Ein benutzerfreundliches ES-Modul mit TypeScript-Typen, Iterator, `readAll`, Writer und Worker.

### Implementierung

- `createIoxModule`;
- `XtfReader` Iterable;
- `IncrementalXtfReader`;
- `XtfWriter`;
- `readAll`/`writeAll`;
- TypeScript Discriminated Unions;
- Worker-Protokoll;
- Browser-/Worker-/Node-dokumentierte Beispiele;
- robuste automatische Handle-Freigabe;
- keine DOM-Abhängigkeit.

### Tests

- Node `node --test`;
- Input als String/Uint8Array/ArrayBuffer;
- Iteratorabbruch und `close()`;
- wiederholte Reader;
- Worker Message Protocol;
- TypeScript Declaration Smoke, soweit lokal ohne zusätzliche schwere Toolchain möglich;
- Browser/Worker minimaler Harness;
- Native/WASM Fixtures.

### Nutzbares Artefakt

Lokal verwendbares npm-Paket `@interlis/iox-wasm` für Browser, Worker und Node.js.

### Commit

```text
phase 8: add idiomatic javascript and worker wasm api
```

---

## Phase 9 – Direkte `ilic-core`-Integration

### Ziel

Optionales modellbewusstes Modul ohne Providerabstraktion.

### Vorbedingung

Der Agent prüft zuerst den tatsächlichen aktuellen Public API Stand des gepinnten `ilic-fork`. Signaturen werden an reale Typen angepasst.

### Implementierung

- CMake Dependency Wege `IOX_ILIC_SOURCE_DIR` und `IOX_FETCH_ILIC`;
- `iox-ilic`;
- `IlicModelIndex`;
- `IlicXtfReader`;
- `IlicXtfWriter`;
- XTF-2.4-Namensauflösung;
- Modellreihenfolge;
- Strict Checks für Topics, Klassen, Attribute;
- kleine `.ili`-Fixtures;
- Build weiterhin ohne `iox-ilic` möglich.

### Tests

- `IOX_ENABLE_ILIC=OFF` vollständig;
- `IOX_ENABLE_ILIC=ON` vollständig;
- model-free und model-aware Events semantisch gleich für gültige Daten;
- unbekannte Klassen/Attribute Lenient vs Strict;
- XTF 2.4 Namespace Mapping;
- Attribute Transfer Order;
- keine Regression im WASM-Basispaket.

### Nutzbares Artefakt

Direkt gegen `ilic-core` integrierter, modellbewusster XTF Reader/Writer sowie weiterhin unabhängiger generischer Kern.

### Commit

```text
phase 9: integrate xtf processing directly with ilic core
```

---

## Phase 10 – Convenience APIs, Beispiele und `iox-dump`

### Ziel

Abgerundete Entwicklererfahrung und demonstrierte Erweiterbarkeit.

### Implementierung

- `BasketReader`/`readBasket()`;
- finale ReaderFactory/WriterFactory;
- `iox-dump` als kleines Integrationsartefakt;
- C++ Beispiele;
- Node Beispiel;
- Custom Format Beispiel;
- vollständige README;
- `docs/extending-formats.md`;
- `docs/wasm.md`.

`iox-dump` darf:

```text
iox-dump input.xtf
iox-dump --events input.xtf
iox-dump --roundtrip input.xtf output.xtf
```

Es ist ein Diagnose-/Beispieltool, kein umfassendes Produkt-CLI.

### Tests

- Dokumentationsbeispiele werden kompiliert/ausgeführt;
- `iox-dump` Smoke;
- Basket Memory Limit;
- Factory Sniffing;
- falsche Extension bei korrektem Content;
- unbekanntes Format.

### Nutzbares Artefakt

Ein gut dokumentiertes, leicht integrierbares SDK mit Native-/WASM-Beispielen und Diagnosewerkzeug.

### Commit

```text
phase 10: complete convenience api examples and dump tool
```

---

## Phase 11 – Finale Conformance-, Coverage- und Fuzz-Härtung

### Ziel

Release-Candidate-Qualität ohne Veröffentlichung und ohne CI/CD.

### Arbeiten

- vollständige Featurematrix prüfen;
- offene TODO/FIXME analysieren;
- alle Coverage-Grenzen erreichen;
- ASan/UBSan;
- Fuzz Targets bauen und kurzen reproduzierbaren Lauf ausführen;
- bekannte Fuzz Crashes als Regression Tests;
- grosse Streaming Fixtures;
- Native/WASM Parität vollständig;
- deterministische Builds/Ausgaben prüfen;
- Dokumentation finalisieren;
- keine Netzwerkabhängigkeit der normalen Tests;
- Lizenz-/Third-Party-Prüfung;
- Public Header Consumer Tests;
- clean build aus frischem Verzeichnis.

### Abschlussbefehle

Mindestens:

```sh
rm -rf build/native build/wasm build/coverage
./scripts/build-native.sh
./scripts/test-native.sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
./scripts/coverage.sh
```

Zusätzlich dokumentierte Sanitizer- und Fuzz-Kommandos.

### Nutzbares Artefakt

Lokaler Release Candidate von `iox-cpp`, `iox-ilic`, `iox-abi` und `@interlis/iox-wasm`.

### Commit

```text
phase 11: finalize conformance coverage and hardening
```

---

# 23. Autonome Git- und Fehlerregeln

## 23.1 Vor jedem Start

```sh
git rev-parse --show-toplevel
git status --short
git branch --show-current
```

Der Repository-Basename muss `iox-cpp` sein. Bei einem anderen Namen darf der Agent nicht destruktiv handeln; er dokumentiert den Fehler und stoppt.

## 23.2 Bestehende Dateien

Das Repository ist vorhanden und kann initiale Benutzerdateien enthalten. Der Agent:

- liest sie;
- integriert sie sinnvoll;
- überschreibt keine fremden Inhalte blind;
- verwendet kein `git reset --hard` gegen nicht vom Agent erzeugte Änderungen;
- verwendet kein `git clean -fdx` ohne präzise Prüfung;
- committet keine fremden Secrets oder lokalen Buildartefakte.

## 23.3 Commit-Regeln

- genau ein Abschlusscommit pro Phase;
- notwendige interne Zwischencommits sind nicht vorgesehen;
- kein Amend abgeschlossener Phasen;
- keine Rebase-/History-Rewrite-Aktionen;
- kein Push;
- Commit enthält Code, Tests und Dokumentation der Phase;
- Working Tree nach Commit sauber.

## 23.4 Drei Reparaturversuche

Ein „materiell anderer“ Versuch ändert Hypothese oder Lösung, zum Beispiel:

1. Codefehler korrigieren;
2. Buildkonfiguration korrigieren;
3. Architekturgrenze korrigieren.

Nicht als eigener Versuch zählen:

- derselbe Test erneut;
- Logging hinzufügen und denselben Fix wiederholen;
- Cache löschen ohne neue Hypothese.

Nach drei gescheiterten Ansätzen:

1. keine Rückfrage stellen;
2. Diagnose und Kommandos in `docs/phase-status.md` eintragen;
3. vom Agent verursachte unvollständige Änderungen der aktuellen Phase entfernen;
4. letzten erfolgreichen Phasencommit erhalten;
5. sauberen Working Tree herstellen, ohne Benutzerarbeit zu zerstören;
6. finalen Blockerbericht ausgeben;
7. stoppen.

---

# 24. Methodenspezifische Akzeptanzkriterien

## `IomObject::detach()`

- kopiert nur bei geteilter Impl;
- erhält alle Metadaten und Reihenfolgen;
- invalidiert Lookup korrekt;
- strukturierte Kinder bleiben COW-sicher;
- getestet mit verschachtelten Objekten.

## `IomObject::deepCopy()`

- rekursive vollständige Kopie;
- spätere Änderung beliebiger Tiefe beeinflusst Original nicht;
- Zyklus wird erkannt und als Fehler behandelt.

## `Reader::feed()`

- kopiert oder konsumiert Input vor Rückkehr;
- beliebige Chunks;
- nach Finish verboten;
- fataler Parserzustand terminal.

## `Reader::next()`

- Event/NeedInput/End eindeutig;
- keine Busy Loop;
- nach End idempotent;
- Diagnostics gehen nicht verloren.

## `XtfReader::detectedVersion()`

- vor Root `nullopt`;
- danach stabil;
- falsche `expectedVersion` fatal;
- 2.2 unsupported.

## `XtfWriter::write()`

- Eventzustand geprüft;
- keine teilweise semantisch ungültige Ausgabe nach fatalem Zustand weiterführen;
- Diagnostics verfügbar;
- gleiche Events ergeben gleiche Bytes.

## `FormatRegistry::createReader()`

- expliziter Formatname vor Sniffing;
- Content Sniff vor Extension bei Konflikt;
- deterministischer Tie-Break;
- verständlicher Fehler ohne Treffer.

## `ExpatParser::feed()`

- C-Callback wirft nicht über C-Frame;
- Byteoffset/Line/Column korrekt;
- DTD/External Entity abgelehnt;
- Limits greifen;
- UTF-8-Split funktioniert.

## `XmlWriter`

- korrektes Escaping;
- Namespace Scope;
- ungültige Aufrufe mit strukturiertem Fehler;
- kein Throw im Destruktor;
- deterministisch.

## `IlicModelIndex`

- Indexaufbau deterministisch;
- keine Ownership des fremden Modells;
- dokumentierte Lebensdauer;
- kein Zugriff nach Ende der Modellebensdauer;
- Klassen/Properties/Topics in Tests.

## C-ABI

- alle Exceptions gefangen;
- Nullargumente geprüft;
- Result Lifetime klar;
- Memory Allocator symmetrisch;
- ABI Version getestet;
- valides UTF-8 JSON.

## JS Wrapper

- Handles in `finally` freigegeben;
- Iteratorabbruch leakt nicht;
- keine DOM Globalen;
- TypeScript vollständig;
- Errors enthalten Code, Message und Location.

---

# 25. Verbotene Implementierungsabkürzungen

Der Coding-Agent darf nicht:

- XTF mit regulären Ausdrücken parsen;
- das gesamte XML-Dokument in einen DOM laden;
- die ganze Datei standardmässig in den Speicher lesen;
- Attribute in `std::map` sortieren und dadurch Reihenfolge verlieren;
- XTF 2.3 und 2.4 in eine unübersichtliche `if(version)`-Klasse pressen;
- Namespace-Präfixe mit Namespace-URIs gleichsetzen;
- fehlende XTF-2.4-Namen erraten;
- `double` als kanonische Speicherung numerischer Transferwerte verwenden;
- unbekannte fachliche Elemente still verwerfen;
- `catch(...)` ohne strukturierte Fehlerumwandlung verwenden;
- Exceptions über C ABI laufen lassen;
- `shared_ptr` ohne COW als behauptete Value-Semantik verkaufen;
- globale mutable Registries mit statischer Initialisierungsreihenfolge verwenden;
- `ilic-core` in `iox-core` oder `iox-xtf` hineinziehen;
- ein abstraktes Model Provider Framework einführen;
- Java in normalen Tests voraussetzen;
- Tests vom Netzwerk abhängig machen;
- Coverage durch Ausschluss eigener schwieriger Dateien schönen;
- Tests deaktivieren, um eine Phase abzuschliessen;
- CI/CD-Dateien erstellen;
- npm publish, Git Push oder Release-Aktionen ausführen;
- den historischen IOM-Code direkt übernehmen;
- ITF implementieren.

---

# 26. Finale Definition of Done

Das Gesamtvorhaben ist abgeschlossen, wenn:

1. alle elf Implementierungsphasen plus Phase 0 committed sind;
2. der Working Tree sauber ist;
3. Native Build und CTest vollständig erfolgreich sind;
4. WASM Build und Node Tests vollständig erfolgreich sind;
5. XTF 2.3 und 2.4 Featurematrix im vereinbarten Scope vollständig ist;
6. Reader und Writer semantische Roundtrips bestehen;
7. Native/WASM-Parität besteht;
8. XTF 2.4 Multi-Geometrien abgedeckt sind;
9. modellfreier Betrieb funktioniert;
10. `iox-ilic` direkt gegen `ilic-core` funktioniert und optional abschaltbar ist;
11. weitere Formate anhand JSON Events demonstriert sind;
12. C-ABI dokumentiert und getestet ist;
13. JavaScript API Browser/Worker/Node-tauglich ist;
14. Coverage mindestens 90 Prozent Line und 85 Prozent Branch für Kernmodule beträgt;
15. Fuzz Targets vorhanden und buildbar sind;
16. ASan/UBSan keine Projektfehler melden;
17. alle öffentlichen API-Pfade getestet sind;
18. `AGENTS.md` und Skills vorhanden und konsistent sind;
19. alle Dokumente aktuell sind;
20. keine CI/CD-Pipeline, Publikation oder Push-Aktion erfolgt ist.

---

# 27. Erwarteter Abschlussbericht des Coding-Agents

Der Agent gibt am Ende einen Bericht mit genau diesen Abschnitten aus:

```text
Implementation summary
Completed phases and commits
Architecture delivered
Native build and test results
WASM build and test results
Coverage results
Sanitizer and fuzz results
XTF 2.3 conformance status
XTF 2.4 conformance status
ilic-core integration status
Known limitations
Repository status
```

Für Testresultate nennt der Agent exakte Kommandos und Exitstatus. Er behauptet keine erfolgreiche Plattformprüfung, die nicht tatsächlich ausgeführt wurde. Nicht verfügbare Plattformen werden als „not executed locally“ ausgewiesen, während der Code und die Buildskripte dennoch vorbereitet sein müssen.

---

# 28. Startprompt für den LLM-Coding-Client

Der folgende Prompt ist für den Coding-Client bestimmt. Die vorliegende Datei soll unter dem Namen `iox-cpp-llm-coding-spec.md` im Repository oder als zugängliche Eingabedatei bereitliegen.

```text
You are the autonomous implementation agent for the existing Git repository named `iox-cpp`.

Your authoritative implementation specification is `iox-cpp-llm-coding-spec.md`. Read it completely before changing any file. Do not skim it. Then inspect the existing repository, its current branch, tracked files, and working-tree state.

Execute the entire roadmap from Phase 0 through Phase 11 autonomously and in order.

Core rules:

1. The repository already exists. Do not initialize or recreate it.
2. Do not ask the user questions. The specification contains the resolved product and architecture decisions.
3. Do not push, publish, create releases, configure remotes, or create CI/CD pipelines.
4. Preserve any pre-existing user work. Never use destructive Git commands against user-owned changes.
5. Create and follow the root `AGENTS.md` and all canonical skills under `.agents/skills/` during Phase 0. Explicitly read the relevant skill before each phase; do not rely on automatic skill discovery.
6. Use the same foundational stack as `edigonzales/ilic-fork`: CMake 3.20+, C++17, CTest, Emscripten 3.1.64, ES modules, and Node.js 18+ tests.
7. Implement only XTF 2.3 and XTF 2.4. Do not implement ITF.
8. The normative core is the ordered `std::variant` Iox event stream and the generic copy-on-write IOM object model.
9. The generic XTF core is model-free. The optional `iox-ilic` module must link directly to concrete `ilic-core` types. Do not create a generic model-provider framework.
10. Preserve attribute order and repeated-value order. Never silently lose fachlich relevant data.
11. Use Expat privately for secure incremental XML parsing and a controlled internal XML writer for deterministic output.
12. Keep Native and WASM semantics identical. No C++ exception may cross the C ABI.
13. Build regular tests without Java and without network access. Java may only be used by an optional differential fixture-generation script.
14. Every phase must deliver a useful, tested artifact, update documentation and phase status, and end with its own Git commit.
15. Use commit messages exactly in the phase-oriented style required by the specification.
16. After committing a successful phase, verify a clean working tree and start the next phase automatically.
17. Never disable, delete, skip, or weaken a valid test to finish a phase.
18. For a blocked phase, attempt at most three materially different repairs. If all fail, document the exact commands, diagnostics, hypotheses, and attempted fixes; restore a clean tree at the last successful phase commit without harming pre-existing user work; then stop with a precise blocker report and no question.

Before Phase 0, run and record:

- `git rev-parse --show-toplevel`
- `git branch --show-current`
- `git status --short`

During Phase 0, verify and pin the exact immutable revisions of Expat, `iox-api`, `iox-ili`, the official INTERLIS test suite, the historical IOM reference, `ilic-fork`, and the normative INTERLIS 2.3/2.4 documents. Record them in `docs/conformance.md`. Never leave a production dependency on a floating branch.

At the end of every phase:

- run all phase-required native tests;
- run all available WASM tests after WASM introduction;
- run the required coverage/sanitizer/fuzz gates for that phase;
- update `docs/phase-status.md` with exact commands and results;
- update `docs/roadmap.md`;
- commit the phase;
- verify `git status --short` is empty.

At the end of Phase 11, perform a clean build from removed build directories and produce the final report using exactly these headings:

Implementation summary
Completed phases and commits
Architecture delivered
Native build and test results
WASM build and test results
Coverage results
Sanitizer and fuzz results
XTF 2.3 conformance status
XTF 2.4 conformance status
ilic-core integration status
Known limitations
Repository status

Begin now. Read the full specification and repository state first, then execute Phase 0 without waiting for confirmation.
```

---

# 29. Prioritätsreihenfolge bei Zielkonflikten

Wenn zwei Ziele kollidieren, gilt:

1. normative XTF-Korrektheit;
2. keine Datenverluste;
3. deterministisches und nachvollziehbares Verhalten;
4. API- und Speicher-Sicherheit;
5. Native/WASM-Parität;
6. Testbarkeit;
7. Streaming und begrenzter Speicher;
8. saubere Modulgrenzen;
9. Entwicklerfreundlichkeit;
10. Performance;
11. minimale Diffgrösse.

Ein kleinerer, korrekter und getesteter Phasenartefakt ist einem grösseren, nur scheinbar vollständigen Artefakt vorzuziehen. Der Gesamtlauf darf aber keine verbindliche spätere Phase auslassen.
