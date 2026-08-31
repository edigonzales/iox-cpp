# C-ABI und WASM-Grenze

Der öffentliche C99-Header ist
[`include/iox/abi/iox.h`](../include/iox/abi/iox.h). Er exportiert nur opake
Reader-, Writer- und Result-Handles. Dieselbe Implementierung steckt in der
nativen Bibliothek `iox-abi` und im Emscripten-Modul.

Eingaben sind nur während des Aufrufs geliehen. Ein Resultat besitzt JSON und
Ausgabebytes bis `iox_result_destroy`; `iox_alloc`/`iox_free` verwalten
WASM-Heap-Puffer. Keine C++-Exception überschreitet die ABI.

## Reader-Zustand

1. Reader für `xtf`, `xtf23`, `xtf24` oder `json-events` erzeugen.
2. Beliebig viele Chunks mit `iox_reader_feed` liefern.
3. `iox_reader_next` bis `event`, `need_input` oder `end` aufrufen.
4. Am Eingabeende `iox_reader_finish` aufrufen und Rest-Events leeren.

Feed/Finish nach dem Abschluss, doppeltes Finish und Next nach dem finalen End
sind ungültig. Destroy-Funktionen akzeptieren Null-Handles. `xtf23`/`xtf24`
wählen einen exakten Dialekt; `xtf` erkennt standardmässig automatisch.

## Writer und Resultate

`iox_writer_write_event_json` akzeptiert ein Objekt aus
[`iox-event/2`](event-json-schema.md). `iox_writer_take_output` übernimmt nur
die seit dem letzten Abruf erzeugten Bytes; `iox_writer_finish` schliesst und
liefert den letzten Chunk. Dadurch bleibt Streaming speicherbegrenzt.

Alle Resultate verwenden `iox-result/2`:

```json
{"schema":"iox-result/2","ok":true,"status":"event","event":{"schema":"iox-event/2","event":"endTransfer"},"error":null,"diagnostics":[]}
```

Fehlerresultate enthalten stabilen Code, Meldung, Quellposition und alle
Diagnosen. Native NDJSON-, C-, Node-, Browser- und Worker-Oberflächen verwenden
denselben Payload ohne Normalisierungsschicht.
