#!/usr/bin/env python3
"""Normalize iox-event/2 NDJSON to the iox-ili differential contract."""

import base64
import json
import sys


def atom(value):
    if value is None:
        return "~"
    return base64.urlsafe_b64encode(value.encode("utf-8")).decode("ascii").rstrip("=")


OPERATIONS = {"insert": 0, "update": 1, "delete": 2, "none": 0}
CONSISTENCIES = {
    "complete": 0,
    "incomplete": 1,
    "inconsistent": 2,
    "adapted": 3,
    "unspecified": 0,
}
BASKET_KINDS = {"full": 0, "update": 1, "initial": 2, "unspecified": 0}


def object_fingerprint(value):
    reference = value.get("reference") or {}
    tag = value["tag"]["interlisName"].rsplit(".", 1)[-1]
    attributes = []
    for attribute in value["attributes"]:
        values = []
        for item in attribute["values"]:
            if item["kind"] == "primitive":
                values.append("p" + atom(item["value"]))
            else:
                values.append("o{" + object_fingerprint(item["value"]) + "}")
        attributes.append(atom(attribute["name"]["interlisName"]) + "=" + ",".join(values))
    attributes.sort()
    return "|".join([
        atom(tag),
        atom(value.get("oid")),
        str(OPERATIONS[value["operation"]]),
        str(CONSISTENCIES[value["consistency"]]),
        atom(reference.get("targetOid")),
        atom(reference.get("targetBasketId")),
        str(reference.get("orderPosition") or 0),
    ]) + "|[" + ";".join(attributes) + "]"


for line in sys.stdin:
    if not line.strip():
        continue
    event = json.loads(line)
    kind = event["event"]
    if kind == "startTransfer":
        print(kind)
    elif kind == "startBasket":
        basket = event["basket"]
        print(kind, atom(basket["topic"]["interlisName"]),
              atom(basket["basketId"]), BASKET_KINDS[basket["kind"]],
              CONSISTENCIES[basket["consistency"]], sep="\t")
    elif kind == "object":
        print(kind, object_fingerprint(event["object"]), sep="\t")
    else:
        print(kind)
