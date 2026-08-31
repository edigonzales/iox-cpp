# Event-JSON-Schema 2

`JsonEventReader`, `JsonEventWriter`, C-ABI und JavaScript-Paket verwenden eine
gemeinsame NDJSON-Repräsentation. Jede Zeile enthält genau ein Event:

```json
{"schema":"iox-event/2","event":"startTransfer","header":{}}
```

`event` ist `startTransfer`, `startBasket`, `object`, `endBasket` oder
`endTransfer`. Schema 1, der frühere `type`-Diskriminator, unbekannte und
doppelte Felder werden abgewiesen. Reihenfolge wird ausschliesslich durch
Arrays dargestellt.

## Namen und Positionen

INTERLIS-Name und optionaler XML-QName bleiben getrennt:

```json
{"interlisName":"Model.Topic.Class","xml":{"namespaceUri":"https://example.test/model","localName":"Class","prefixHint":"m"}}
```

Ohne bekannten QName ist `xml` null. Eine Position enthält `sourceName`,
`byteOffset`, `line` und `column`. Erweiterungselemente bewahren QName,
geordnete Attribute, Text und rekursive Kinder.

## Transfer, Korb und Objekt

Ein Transferheader enthält `version`, `sender`, optional `comment` sowie die
immer vorhandenen Arrays `models`, `oidSpaces` und `extensions`. Ein
`startBasket` enthält Topic-Name, `basketId`, Kind, Konsistenz, optionale
Start-/Endzustände, Domains, Topics, Extensions und Position.

```json
{
  "schema":"iox-event/2",
  "event":"object",
  "object":{
    "tag":{"interlisName":"Model.Topic.Class","xml":null},
    "oid":"T1",
    "operation":"insert",
    "consistency":"complete",
    "reference":null,
    "location":{"sourceName":"data.xtf","byteOffset":120,"line":6,"column":5},
    "attributes":[{"name":{"interlisName":"value","xml":null},"values":[{"kind":"primitive","value":"001.2300"}]}]
  }
}
```

`oid` ist optional. Operationen sind `insert`, `update`, `delete` und `none`.
Referenzen besitzen optional `targetOid`, `targetBasketId` und eine
nichtnegative `orderPosition`. Primitive bleiben lexikalische Strings;
verschachtelte Objekte verwenden rekursiv dieselbe Struktur.

Endevents besitzen keinen Payload:

```json
{"schema":"iox-event/2","event":"endBasket"}
{"schema":"iox-event/2","event":"endTransfer"}
```

## Resultate und Diagnosen

C-ABI-Resultate verwenden `iox-result/2`. `event` und `error` sind bei
Abwesenheit null. Diagnosen enthalten Severity, stabilen Code, erklärende
Meldung, Position und `contextPath`. Severities sind `info`, `warning`,
`error` und `fatal`. Codes sind maschinenlesbarer Vertrag; Meldungstexte dürfen
sich ändern.
