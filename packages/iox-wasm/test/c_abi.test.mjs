import assert from 'node:assert/strict';
import { test } from 'node:test';

import { createIoxModule } from '../index.js';

const encoder = new TextEncoder();

function cString(native, value) {
  const bytes = encoder.encode(value);
  const pointer = native._iox_alloc(bytes.length + 1);
  native.HEAPU8.set(bytes, pointer);
  native.HEAPU8[pointer + bytes.length] = 0;
  return { pointer, size: bytes.length };
}

function freeString(native, value) {
  native._iox_free(value.pointer);
}

function result(native, resultPointer) {
  const handle = native.HEAP32[resultPointer >> 2];
  assert.notEqual(handle, 0);
  const json = native.UTF8ToString(native._iox_result_json(handle));
  const status = native._iox_result_status(handle);
  const size = native._iox_result_size(handle);
  const bytesPointer = native._iox_result_bytes(handle);
  const bytes = bytesPointer && size > 0
    ? native.HEAPU8.slice(bytesPointer, bytesPointer + size)
    : new Uint8Array(0);
  native._iox_result_destroy(handle);
  return { json: JSON.parse(json), status, bytes };
}

test('WASM C ABI preserves incremental reader states and payloads', async () => {
  const mod = await createIoxModule();
  const native = mod._native;
  assert.equal(native._iox_abi_version(), 1);
  assert.equal(typeof native._iox_writer_take_output, 'function');

  const format = cString(native, 'json-events');
  const reader = native._iox_reader_create(format.pointer, 0);
  freeString(native, format);
  assert.notEqual(reader, 0);

  const event = cString(native, '{"event":"startTransfer","sender":"wasm"}');
  const half = Math.floor(event.size / 2);
  const out = native._iox_alloc(4);
  assert.equal(native._iox_reader_feed(reader, event.pointer, half), 0);
  assert.equal(native._iox_reader_next(reader, out), 2);
  const needInput = result(native, out);
  assert.equal(needInput.status, 2);
  assert.equal(needInput.json.status, 'need_input');
  assert.deepEqual([...needInput.bytes], []);

  assert.equal(native._iox_reader_feed(reader, event.pointer + half, event.size - half), 0);
  const newline = cString(native, '\n');
  assert.equal(native._iox_reader_feed(reader, newline.pointer, 1), 0);
  assert.equal(native._iox_reader_next(reader, out), 1);
  const eventResult = result(native, out);
  assert.equal(eventResult.status, 1);
  assert.equal(eventResult.json.event.event, 'startTransfer');
  assert.equal(eventResult.json.event.sender, 'wasm');
  assert.equal(native._iox_reader_finish(reader), 0);
  assert.equal(native._iox_reader_next(reader, out), 3);
  assert.equal(result(native, out).json.status, 'end');

  native._iox_free(event.pointer);
  freeString(native, newline);
  native._iox_free(out);
  native._iox_reader_destroy(reader);
});

test('WASM C ABI writes output chunks and returns structured errors', async () => {
  const mod = await createIoxModule();
  const native = mod._native;
  const format = cString(native, 'json-events');
  const writer = native._iox_writer_create(format.pointer, 0);
  freeString(native, format);
  assert.notEqual(writer, 0);
  const out = native._iox_alloc(4);

  const start = cString(native, '{"event":"startTransfer","sender":"wasm"}');
  assert.equal(native._iox_writer_write_event_json(writer, start.pointer, start.size, out), 0);
  assert.equal(result(native, out).status, 0);
  assert.equal(native._iox_writer_take_output(writer, out), 0);
  const first = result(native, out);
  assert.ok(first.bytes.length > 0);
  assert.match(new TextDecoder().decode(first.bytes), /StartTransfer/);
  native._iox_free(start.pointer);

  const invalid = cString(native, '{');
  assert.equal(native._iox_writer_write_event_json(writer, invalid.pointer, invalid.size, out), -1);
  const error = result(native, out);
  assert.equal(error.status, -1);
  assert.equal(error.json.ok, false);
  assert.equal(error.json.error.code, 'json.parse_error');
  native._iox_free(invalid.pointer);

  assert.equal(native._iox_writer_finish(writer, out), 0);
  assert.deepEqual([...result(native, out).bytes], []);
  native._iox_free(out);
  native._iox_writer_destroy(writer);
});
