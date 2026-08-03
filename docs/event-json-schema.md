# Event JSON schema 2

`JsonEventReader`, `JsonEventWriter`, the C ABI, and the JavaScript package use
one canonical NDJSON representation. Every line is exactly one event object
with these required envelope fields:

```json
{"schema":"iox-event/2","event":"startTransfer","header":{}}
```

`event` is one of `startTransfer`, `startBasket`, `object`, `endBasket`, or
`endTransfer`. Schema 1 and the former `type` discriminator are rejected.
Unknown and duplicate JSON fields are rejected. Attribute and repeated-value
order is represented only by arrays.

## Names, locations, and extensions

An INTERLIS name and its optional XML QName are kept separate:

```json
{
  "interlisName": "Model.Topic.Class",
  "xml": {
    "namespaceUri": "https://example.test/model",
    "localName": "Class",
    "prefixHint": "m"
  }
}
```

`xml` is `null` when no QName is known. A source location always has
`sourceName`, `byteOffset`, `line`, and `column`. Extension elements preserve
their QName, ordered attributes, text, and recursive children:

```json
{
  "name": {"namespaceUri":"urn:ext","localName":"meta","prefixHint":"e"},
  "attributes": [
    {"name":{"namespaceUri":"","localName":"flag","prefixHint":""},"value":"yes"}
  ],
  "text": "payload",
  "children": []
}
```

## Transfer and basket events

```json
{
  "schema": "iox-event/2",
  "event": "startTransfer",
  "header": {
    "version": "2.3",
    "sender": "MyApp",
    "comment": "optional",
    "models": [{
      "name": "Model",
      "version": "2026-01-01",
      "uri": "https://example.test/model.ili",
      "xmlNamespace": {
        "namespaceUri": "https://example.test/model",
        "localName": "Model",
        "prefixHint": "m"
      }
    }],
    "oidSpaces": [{"name":"UUID","domain":"INTERLIS.UUIDOID"}],
    "extensions": []
  }
}
```

`comment`, model `version`, and model `uri` are optional. All arrays are
required, including when empty.

```json
{
  "schema": "iox-event/2",
  "event": "startBasket",
  "basket": {
    "topic": {"interlisName":"Model.Topic","xml":null},
    "basketId": "B1",
    "kind": "full",
    "consistency": "complete",
    "startState": "optional",
    "endState": "optional",
    "domains": [],
    "topics": [],
    "extensions": [],
    "location": {"sourceName":"data.xtf","byteOffset":80,"line":4,"column":3}
  }
}
```

`startState` and `endState` are optional. Basket kinds are `full`, `update`,
`initial`, and `unspecified`. Consistency values are `complete`, `incomplete`,
`inconsistent`, `adapted`, and `unspecified`.

## Object event and IOM values

```json
{
  "schema": "iox-event/2",
  "event": "object",
  "object": {
    "tag": {"interlisName":"Model.Topic.Class","xml":null},
    "oid": "T1",
    "operation": "insert",
    "consistency": "complete",
    "reference": null,
    "location": {"sourceName":"data.xtf","byteOffset":120,"line":6,"column":5},
    "attributes": [{
      "name": {"interlisName":"value","xml":null},
      "values": [
        {"kind":"primitive","value":"001.2300"},
        {"kind":"object","value":{
          "tag":{"interlisName":"Model.Topic.Structure","xml":null},
          "operation":"none",
          "consistency":"unspecified",
          "reference":null,
          "location":{"sourceName":"","byteOffset":0,"line":0,"column":0},
          "attributes":[]
        }}
      ]
    }]
  }
}
```

`oid` is optional. `operation` is `insert`, `update`, `delete`, or `none`.
References are either `null` or an object with optional `targetOid`,
`targetBasketId`, and unsigned `orderPosition`. Primitives are always lexical
strings: JSON numbers, booleans, dates, and null are not IOM primitive values.
Nested objects use the same representation recursively.

End events contain no payload:

```json
{"schema":"iox-event/2","event":"endBasket"}
{"schema":"iox-event/2","event":"endTransfer"}
```

## ABI result and diagnostics

Every C-ABI result uses `iox-result/2` and contains stable object members;
`event` and `error` are `null` when absent:

```json
{
  "schema": "iox-result/2",
  "ok": false,
  "status": "error",
  "event": null,
  "error": {
    "code": "xml.malformed",
    "message": "XML parse error",
    "location": {"sourceName":"data.xtf","byteOffset":123,"line":5,"column":19}
  },
  "diagnostics": [{
    "severity": "fatal",
    "code": "xml.malformed",
    "message": "XML parse error",
    "location": {"sourceName":"data.xtf","byteOffset":123,"line":5,"column":19},
    "contextPath": []
  }]
}
```

Severities are `info`, `warning`, `error`, and `fatal`. Diagnostic codes are
stable machine-readable identifiers; messages are explanatory and may evolve.
