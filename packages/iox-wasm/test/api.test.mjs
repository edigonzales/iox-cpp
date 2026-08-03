import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  createIoxModule,
  IncrementalXtfReader,
  XtfReader,
  XtfWriter,
  readAll,
  writeAll,
} from '../index.js';
import { handleWorkerMessage } from '../worker.js';

const fixture =
  '<?xml version="1.0"?><ili:TRANSFER xmlns:ili="http://www.interlis.ch/INTERLIS2.3">' +
  '<ili:HEADERSECTION><ili:SENDER>Phase8</ili:SENDER></ili:HEADERSECTION>' +
  '</ili:TRANSFER>';

const location = { sourceName: '', byteOffset: 0, line: 0, column: 0 };
const name = (interlisName) => ({ interlisName, xml: null });

const events = [
  {
    schema: 'iox-event/2', event: 'startTransfer',
    header: { version: '2.3', sender: 'Phase8', models: [], oidSpaces: [], extensions: [] },
  },
  {
    schema: 'iox-event/2', event: 'startBasket',
    basket: {
      topic: name('M.T'), basketId: 'B1', kind: 'full', consistency: 'complete',
      domains: [], topics: [], extensions: [], location,
    },
  },
  {
    schema: 'iox-event/2', event: 'object',
    object: {
      tag: name('M.T.C'), oid: 'T1', operation: 'insert', consistency: 'complete',
      reference: null, location,
      attributes: [
        { name: name('First'), values: [{ kind: 'primitive', value: 'one' }] },
        { name: name('Repeated'), values: [
          { kind: 'primitive', value: 'two' }, { kind: 'primitive', value: 'three' },
        ] },
        { name: name('Nested'), values: [{ kind: 'object', value: {
          tag: name('M.T.S'), operation: 'none', consistency: 'unspecified',
          reference: null, location, attributes: [
            { name: name('Value'), values: [{ kind: 'primitive', value: 'four' }] },
          ],
        } }] },
      ],
    },
  },
  { schema: 'iox-event/2', event: 'endBasket' },
  { schema: 'iox-event/2', event: 'endTransfer' },
];

test('idiomatic reader accepts string, bytes, ArrayBuffer, and iterator close', async () => {
  const mod = await createIoxModule();
  const encoded = new TextEncoder().encode(fixture);
  const variants = [fixture, encoded, encoded.buffer];
  for (const input of variants) {
    const reader = new XtfReader(mod, input);
    assert.deepEqual(reader.readAll().map((event) => event.event), ['startTransfer', 'endTransfer']);
    assert.equal(reader.readAll()[0].header.sender, 'Phase8');
    assert.deepEqual(reader.diagnostics(), []);
    for (const event of reader) {
      assert.ok(event.event);
      break;
    }
    reader.close();
  }
});

test('incremental reader works across arbitrary chunks and repeated sessions', async () => {
  const mod = await createIoxModule();
  const encoded = new TextEncoder().encode(fixture);
  const reader = new IncrementalXtfReader(mod);
  const output = [];
  for (let offset = 0; offset < encoded.length; offset += 7) {
    output.push(...reader.feed(encoded.subarray(offset, offset + 7)));
  }
  output.push(...reader.finish());
  assert.deepEqual(output.map((event) => event.event), ['startTransfer', 'endTransfer']);
  reader.close();

  for (let i = 0; i < 5; i += 1) {
    const repeated = new XtfReader(mod, fixture);
    assert.equal(repeated.readAll().length, 2);
    repeated.close();
  }
});

test('writer and convenience functions preserve event semantics', async () => {
  const mod = await createIoxModule();
  const output = writeAll(mod, events, { version: '2.3', sender: 'IgnoredByEvent' });
  assert.ok(output.length > 0);
  const roundtrip = readAll(mod, output);
  assert.equal(roundtrip[0].header.sender, 'Phase8');
  assert.deepEqual(roundtrip.map((event) => event.event), events.map((event) => event.event));
  assert.deepEqual(roundtrip[2].object.attributes.map((attribute) => attribute.name.interlisName), [
    'First', 'Repeated', 'Nested',
  ]);
  assert.deepEqual(roundtrip[2].object.attributes[1].values.map((value) => value.value),
    ['two', 'three']);
  assert.equal(roundtrip[2].object.attributes[2].values[0].value
    .attributes[0].values[0].value, 'four');

  const writer = new XtfWriter(mod, { version: '2.4' });
  writer.write(events[0]);
  for (const event of events.slice(1)) writer.write(event);
  const second = writer.finish();
  assert.ok(second.length > 0);
  assert.deepEqual(readAll(mod, second).map((event) => event.event), events.map((event) => event.event));
  writer.close();
});

test('worker protocol returns request IDs, events, transferable bytes, and errors', async () => {
  const messages = [];
  const post = (message, transfer = []) => messages.push({ message, transfer });
  await handleWorkerMessage({ type: 'init', requestId: 10 }, post);
  assert.equal(messages.at(-1).message.ok, true);
  assert.equal(messages.at(-1).message.requestId, 10);

  await handleWorkerMessage({ type: 'readAll', requestId: 11, input: fixture }, post);
  assert.equal(messages.at(-1).message.ok, true);
  assert.deepEqual(messages.at(-1).message.events.map((event) => event.event), [
    'startTransfer', 'endTransfer'
  ]);

  await handleWorkerMessage({
    type: 'writeAll', requestId: 12, events, options: { version: '2.3' }
  }, post);
  assert.equal(messages.at(-1).message.ok, true);
  assert.ok(messages.at(-1).message.bytes.byteLength > 0);
  assert.equal(messages.at(-1).transfer.length, 1);

  await handleWorkerMessage({ type: 'unknown', requestId: 13 }, post);
  assert.equal(messages.at(-1).message.ok, false);
  assert.equal(messages.at(-1).message.error.code, 'invalid_argument');

  await handleWorkerMessage({ type: 'close', requestId: 14 }, post);
  assert.equal(messages.at(-1).message.ok, true);
});
