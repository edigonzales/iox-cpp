# Event JSON Schema

This document describes the canonical JSON representation of `IoxEvent`
objects produced by `JsonEventReader`/`JsonEventWriter`.

Each event is serialized as a single JSON object on its own line (NDJSON).

## Common Fields

All events share these fields:

```json
{
  "type": "<EventType>",
  ...
}
```

`type` is one of: `StartTransfer`, `StartBasket`, `Object`, `EndBasket`, `EndTransfer`.

## StartTransferEvent

```json
{
  "type": "StartTransfer",
  "sender": "SenderName",
  "comment": "Optional comment",
  "iliVersion": "2.3",
  "software": "iox-cpp",
  "date": "2025-01-01",
  "version": 23
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| sender | string | no | Sender identifier |
| comment | string | no | Transfer comment |
| iliVersion | string | no | INTERLIS version string |
| software | string | no | Generating software |
| date | string | no | Transfer date |
| version | integer | no | XTF version: 23 or 24 |

## StartBasketEvent

```json
{
  "type": "StartBasket",
  "basketType": "Model.Topic.Basket",
  "bid": "BID001",
  "consistency": "complete",
  "operation": "insert",
  "oidDomain": 1,
  "startState": "initial",
  "endState": "final",
  "kind": "DATA",
  "domains": ["domain1", "domain2"]
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| basketType | string | yes | INTERLIS scoped basket type name |
| bid | string | yes | Basket ID (BID) |
| consistency | string | no | "complete" or "incomplete" |
| operation | string | no | "insert", "update", "delete" |
| oidDomain | integer | no | OID domain number |
| startState | string | no | XTF 2.4 start state |
| endState | string | no | XTF 2.4 end state |
| kind | string | no | XTF 2.4 Kind |
| domains | [string] | no | User-defined domains |

## ObjectEvent

```json
{
  "type": "Object",
  "operation": "insert",
  "objectId": "TID001",
  "consistency": "complete",
  "refBid": "BID_REF",
  "refOrderPos": "1",
  "object": {
    "tag": "Model.Topic.ClassName",
    "ref": "REF_VALUE",
    "bid": "BID_VALUE",
    "orderPos": "1",
    "attrs": [
      {
        "name": "AttributeName",
        "value": "primitive text value",
        "ref": "REF_VALUE",
        "bid": "BID_VALUE",
        "orderPos": "1",
        "values": ["repeated", "values"]
      }
    ]
  }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| operation | string | yes | "insert", "update", "delete" |
| objectId | string | yes | Object TID |
| consistency | string | no | Object-level consistency |
| refBid | string | no | Reference BID |
| refOrderPos | string | no | Reference ORDER_POS |
| object | object | yes | The IOM object (see below) |

### IomObject

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| tag | string | yes | INTERLIS class name |
| ref | string | no | Object-level REF |
| bid | string | no | Object-level BID |
| orderPos | string | no | Object-level ORDER_POS |
| attrs | [IomAttribute] | no | Ordered attributes |

### IomAttribute

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| name | string | yes | Attribute INTERLIS name |
| value | variant | no | Single primitive or structured value |
| values | [variant] | no | Multiple values (repeated) |
| ref | string | no | Reference metadata |
| bid | string | no | Bid metadata |
| orderPos | string | no | Order position |

A `value` can be:
- A JSON string (text primitive)
- A JSON number (integer or decimal)
- A JSON boolean
- A JSON null
- A JSON object (nested `IomObject`)

## EndBasketEvent

```json
{
  "type": "EndBasket",
  "bid": "BID001"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| bid | string | yes | Basket ID being closed |

## EndTransferEvent

```json
{
  "type": "EndTransfer"
}
```

No additional fields.

## Error Result

When an error occurs, the ABI produces:

```json
{
  "ok": false,
  "status": "error",
  "error": {
    "code": "xml.malformed",
    "message": "XML parse error: ...",
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

## Diagnostic

```json
{
  "severity": "Warning",
  "code": "xtf.unknown.element",
  "message": "Unknown element: ili:CustomExtension",
  "location": {
    "sourceName": "data.xtf",
    "byteOffset": 456,
    "line": 12,
    "column": 7
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| severity | string | "Warning", "Error", or "Fatal" |
| code | string | Stable error code |
| message | string | Human-readable description |
| location | object | Optional source location |
