# Eigene Formate ergänzen

Zusätzliche Transferformate implementieren die öffentlichen `Reader`- und/oder
`Writer`-Interfaces und werden explizit in einer `FormatRegistry` registriert.
Alle Formate sind statisch gebunden; dynamisches Plugin-Laden gibt es nicht.

```cpp
class MyReader final : public iox::Reader {
public:
    ReadOutcome next() override;
    void feed(iox::ByteView data) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<iox::Diagnostic> takeDiagnostics() override;
};
```

Ein Writer konsumiert `IoxEvent`, schreibt in einen `OutputSink`, implementiert
`flush()`/`close()` und gibt Diagnosen geordnet zurück. Nach einem terminalen
Fehler darf er keine Ausgabe mehr akzeptieren.

```cpp
void registerMyFormat(iox::FormatRegistry& registry) {
    iox::FormatEntry entry;
    entry.name = "my-format";
    entry.description = "Eigenes Transferformat";
    entry.extensions = {".myf"};
    entry.scoreSniffer = [](iox::ByteView first) { return looksLikeMyFormat(first) ? 100 : 0; };
    entry.readerFactory = [] { return std::make_unique<MyReader>(); };
    entry.writerFactory = [](std::shared_ptr<iox::OutputSink> sink) {
        return std::make_unique<MyWriter>(std::move(sink));
    };
    registry.addFormat(std::move(entry));
}
```

Sniffer liefern 0 bis 100 und dürfen keine Eingabe verbrauchen. Gleiche
Höchstwerte sind mehrdeutig und müssen abgewiesen werden. Die
Factory-Registrierung geschieht explizit beim Programmstart; globale
Konstruktoren sind nicht zulässig.

Tests müssen Chunk-Grenzen, Eventreihenfolge, terminale Fehler,
deterministische Writerbytes, Extension-Auswahl und einen öffentlichen
Consumer abdecken. Das vollständige Beispiel liegt unter
[`examples/cpp-custom-format.cpp`](../examples/cpp-custom-format.cpp).
