import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Worker } from 'node:worker_threads';

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
  '<ili:HEADERSECTION VERSION="2.3" SENDER="Phase8">' +
  '<ili:MODELS><ili:MODEL NAME="M" VERSION="1" URI="urn:m"/></ili:MODELS>' +
  '</ili:HEADERSECTION><ili:DATASECTION/>' +
  '</ili:TRANSFER>';

const location = { sourceName: '', byteOffset: 0, line: 0, column: 0 };
const name = (interlisName) => ({ interlisName, xml: null });

const events = [
  {
    schema: 'iox-event/2', event: 'startTransfer',
    header: {
      version: '2.3', sender: 'Phase8',
      models: [{
        name: 'M', version: '1', uri: 'urn:m',
        xmlNamespace: { namespaceUri: '', localName: '', prefixHint: '' },
      }],
      oidSpaces: [], extensions: [],
    },
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

const parityEvents = [
  {
    schema: 'iox-event/2', event: 'startTransfer',
    header: {
      version: '2.3', sender: 'Browser', models: [{
        name: 'M', version: '1', uri: 'urn:m',
        xmlNamespace: { namespaceUri: '', localName: '', prefixHint: '' },
      }], oidSpaces: [], extensions: [],
    },
  },
  { schema: 'iox-event/2', event: 'endTransfer' },
];

const parityXtf = '<?xml version="1.0" encoding="UTF-8"?>' +
  '<TRANSFER xmlns="http://www.interlis.ch/INTERLIS2.3">' +
  '<HEADERSECTION VERSION="2.3" SENDER="Browser"><MODELS>' +
  '<MODEL NAME="M" VERSION="1" URI="urn:m"/></MODELS></HEADERSECTION>' +
  '<DATASECTION/></TRANSFER>';

function concatBytes(chunks) {
  const size = chunks.reduce((sum, chunk) => sum + chunk.length, 0);
  const result = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) {
    result.set(chunk, offset);
    offset += chunk.length;
  }
  return result;
}

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
  assert.throws(() => reader.finish(), { code: 'api.invalid_state' });
  assert.throws(() => reader.feed(''), { code: 'api.invalid_state' });
  reader.close();

  for (let i = 0; i < 5; i += 1) {
    const repeated = new XtfReader(mod, fixture);
    assert.equal(repeated.readAll().length, 2);
    repeated.close();
  }

  const malformed = new IncrementalXtfReader(mod, { sourceName: 'broken.xtf' });
  assert.throws(() => malformed.feed('<ili:TRANSFER>'), (error) => {
    assert.equal(error.code, 'xml.malformed');
    assert.equal(error.diagnostics[0].location.sourceName, 'broken.xtf');
    return true;
  });
  malformed.close();
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

  const events24 = structuredClone(events);
  events24[0].header.version = '2.4';
  delete events24[0].header.models[0].version;
  delete events24[0].header.models[0].uri;
  events24[0].header.models[0].xmlNamespace = {
    namespaceUri: 'urn:m', localName: 'M', prefixHint: 'm',
  };
  const attachModelQName = (iomName) => {
    iomName.xml = {
      namespaceUri: 'urn:m',
      localName: iomName.interlisName.startsWith('M.')
        ? iomName.interlisName.slice(2) : iomName.interlisName,
      prefixHint: 'm',
    };
  };
  attachModelQName(events24[1].basket.topic);
  attachModelQName(events24[2].object.tag);
  for (const attribute of events24[2].object.attributes) {
    attachModelQName(attribute.name);
    for (const value of attribute.values) {
      if (value.kind !== 'object') continue;
      attachModelQName(value.value.tag);
      for (const nestedAttribute of value.value.attributes) {
        attachModelQName(nestedAttribute.name);
      }
    }
  }
  const writer = new XtfWriter(mod, { version: '2.4', pretty: false });
  writer.write(events24[0]);
  for (const event of events24.slice(1)) writer.write(event);
  const second = writer.finish();
  assert.ok(second.length > 0);
  assert.match(new TextDecoder().decode(second), /ili:transfer/);
  assert.deepEqual(readAll(mod, second).map((event) => event.event),
    events24.map((event) => event.event));
  writer.close();

  const streaming = new XtfWriter(mod, { version: '2.3', pretty: false });
  const chunks = [];
  for (const event of events) {
    streaming.write(event);
    chunks.push(streaming.takeOutput());
  }
  chunks.push(streaming.finish());
  assert.throws(() => streaming.finish(), { code: 'api.invalid_state' });
  assert.throws(() => streaming.takeOutput(), { code: 'api.invalid_state' });
  streaming.close();
  const batch = writeAll(mod, events, { version: '2.3', pretty: false });
  assert.deepEqual(concatBytes(chunks), batch);
  assert.equal(new TextDecoder().decode(
    writeAll(mod, parityEvents, { version: '2.3', pretty: false })), parityXtf);
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
    type: 'writeAll', requestId: 12, events: parityEvents,
    options: { version: '2.3', pretty: false }
  }, post);
  assert.equal(messages.at(-1).message.ok, true);
  assert.equal(new TextDecoder().decode(messages.at(-1).message.bytes), parityXtf);
  assert.equal(messages.at(-1).transfer.length, 1);

  await handleWorkerMessage({ type: 'unknown', requestId: 13 }, post);
  assert.equal(messages.at(-1).message.ok, false);
  assert.equal(messages.at(-1).message.error.code, 'api.invalid_argument');

  await handleWorkerMessage({
    type: 'readerCreate', requestId: 14, streamId: 'reader-1'
  }, post);
  const encoded = new TextEncoder().encode(fixture);
  await handleWorkerMessage({
    type: 'readerFeed', requestId: 15, streamId: 'reader-1',
    input: encoded.subarray(0, 31),
  }, post);
  assert.equal(messages.at(-1).message.ok, true);
  await handleWorkerMessage({
    type: 'readerFeed', requestId: 16, streamId: 'reader-1',
    input: encoded.subarray(31),
  }, post);
  const streamedEvents = [...messages.at(-1).message.events];
  await handleWorkerMessage({
    type: 'readerFinish', requestId: 17, streamId: 'reader-1'
  }, post);
  streamedEvents.push(...messages.at(-1).message.events);
  assert.deepEqual(streamedEvents.map((event) => event.event),
    ['startTransfer', 'endTransfer']);

  await handleWorkerMessage({
    type: 'readerCreate', requestId: 18, streamId: 'cancel-reader'
  }, post);
  await handleWorkerMessage({
    type: 'readerClose', requestId: 19, streamId: 'cancel-reader'
  }, post);
  assert.equal(messages.at(-1).message.ok, true);
  await handleWorkerMessage({
    type: 'writerCreate', requestId: 20, streamId: 'cancel-writer',
    options: { version: '2.3' },
  }, post);
  await handleWorkerMessage({
    type: 'writerClose', requestId: 21, streamId: 'cancel-writer'
  }, post);
  assert.equal(messages.at(-1).message.ok, true);

  await handleWorkerMessage({ type: 'close', requestId: 22 }, post);
  assert.equal(messages.at(-1).message.ok, true);
});

test('module worker executes streaming reader/writer and diagnostic parity', async () => {
  const worker = new Worker(new URL('../worker.js', import.meta.url), {
    type: 'module',
  });
  let requestId = 0;
  const request = (payload) => new Promise((resolve, reject) => {
    requestId += 1;
    const timer = setTimeout(() => reject(new Error('worker timeout')), 10000);
    const listener = (message) => {
      if (message.requestId !== requestId) return;
      clearTimeout(timer);
      worker.off('message', listener);
      resolve(message);
    };
    worker.on('message', listener);
    worker.postMessage({ ...payload, requestId });
  });
  try {
    const initialized = await request({ type: 'init' });
    assert.equal(initialized.ok, true);
    assert.equal(initialized.abiVersion, 2);
    assert.equal((await request({
      type: 'readerCreate', streamId: 'actual-reader'
    })).ok, true);
    const encoded = new TextEncoder().encode(fixture);
    const first = await request({
      type: 'readerFeed', streamId: 'actual-reader', input: encoded.subarray(0, 17)
    });
    const second = await request({
      type: 'readerFeed', streamId: 'actual-reader', input: encoded.subarray(17)
    });
    const finished = await request({
      type: 'readerFinish', streamId: 'actual-reader'
    });
    assert.deepEqual([
      ...first.events, ...second.events, ...finished.events,
    ].map((event) => event.event),
      ['startTransfer', 'endTransfer']);

    const written = await request({
      type: 'writeAll', events: parityEvents,
      options: { version: '2.3', pretty: false },
    });
    assert.equal(new TextDecoder().decode(written.bytes), parityXtf);

    const malformed = await request({ type: 'readAll', input: '<ili:TRANSFER>' });
    assert.equal(malformed.ok, false);
    assert.equal(malformed.error.code, 'xml.malformed');
  } finally {
    await worker.terminate();
  }
});
