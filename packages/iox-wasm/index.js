/**
 * @interlis/iox-wasm — INTERLIS XTF 2.3/2.4 Reader/Writer
 *
 * The public API uses the canonical `event` discriminator documented by the
 * C ABI.  All native handles are released on normal and exceptional paths.
 */

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

const EVENT_TYPES = new Set([
  'startTransfer', 'startBasket', 'object', 'endBasket', 'endTransfer'
]);

/** @typedef {{code: string, message: string, severity?: string, location?: object}} Diagnostic */

export class IoxError extends Error {
  /** @param {string} message @param {string} [code] @param {Diagnostic[]} [diagnostics] */
  constructor(message, code = 'internal_error', diagnostics = []) {
    super(message);
    this.name = 'IoxError';
    this.code = code;
    this.diagnostics = diagnostics;
  }
}

function inputBytes(input) {
  if (typeof input === 'string') return textEncoder.encode(input);
  if (input instanceof Uint8Array) return input;
  if (input instanceof ArrayBuffer) return new Uint8Array(input);
  if (ArrayBuffer.isView(input)) {
    return new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
  }
  throw new TypeError('input must be a UTF-8 string, Uint8Array, or ArrayBuffer');
}

function optionsJson(options) {
  return JSON.stringify(options ?? {});
}

function allocateCString(native, value) {
  const bytes = textEncoder.encode(value);
  const pointer = native._iox_alloc(bytes.length + 1);
  if (!pointer) throw new IoxError('Unable to allocate WASM input buffer', 'internal_error');
  native.HEAPU8.set(bytes, pointer);
  native.HEAPU8[pointer + bytes.length] = 0;
  return { pointer, size: bytes.length };
}

function withCString(native, value, callback) {
  const allocation = allocateCString(native, value);
  try {
    return callback(allocation);
  } finally {
    native._iox_free(allocation.pointer);
  }
}

function allocateBytes(native, bytes) {
  if (bytes.length === 0) return 0;
  const pointer = native._iox_alloc(bytes.length);
  if (!pointer) throw new IoxError('Unable to allocate WASM input buffer', 'internal_error');
  native.HEAPU8.set(bytes, pointer);
  return pointer;
}

function parseResult(native, resultPointer, callStatus) {
  const handle = resultPointer ? native.HEAP32[resultPointer >> 2] : 0;
  try {
    if (!handle) {
      throw new IoxError(`C ABI call failed with status ${callStatus}`, 'internal_error');
    }
    const jsonPointer = native._iox_result_json(handle);
    const json = jsonPointer ? JSON.parse(native.UTF8ToString(jsonPointer)) : {};
    const status = native._iox_result_status(handle);
    const size = native._iox_result_size(handle);
    const bytesPointer = native._iox_result_bytes(handle);
    const bytes = bytesPointer && size > 0
      ? native.HEAPU8.slice(bytesPointer, bytesPointer + size)
      : new Uint8Array(0);
    return { status, json, bytes };
  } finally {
    if (handle) native._iox_result_destroy(handle);
  }
}

function callResult(mod, call) {
  const native = mod._native;
  const resultPointer = native._iox_alloc(4);
  if (!resultPointer) throw new IoxError('Unable to allocate result pointer', 'internal_error');
  native.HEAP32[resultPointer >> 2] = 0;
  try {
    const callStatus = call(resultPointer);
    return parseResult(native, resultPointer, callStatus);
  } finally {
    native._iox_free(resultPointer);
  }
}

function throwResultError(result) {
  const error = result.json?.error ?? {};
  throw new IoxError(
    error.message ?? `C ABI operation failed with status ${result.status}`,
    error.code ?? 'internal_error',
    result.json?.diagnostics ?? []
  );
}

function addDiagnostics(target, result) {
  if (Array.isArray(result.json?.diagnostics)) target.push(...result.json.diagnostics);
}

function resultEvent(result) {
  if (result.status !== 1 || !result.json?.event) return null;
  const event = result.json.event;
  if (!EVENT_TYPES.has(event.event)) {
    throw new IoxError('Unknown event discriminator returned by ABI', 'json.parse_error');
  }
  return event;
}

function createHandle(mod, kind, format, options) {
  const native = mod._native;
  const optionText = optionsJson(options);
  return withCString(native, format, (formatAllocation) =>
    withCString(native, optionText, (optionsAllocation) => {
      const handle = kind === 'reader'
        ? native._iox_reader_create(formatAllocation.pointer, optionsAllocation.pointer)
        : native._iox_writer_create(formatAllocation.pointer, optionsAllocation.pointer);
      if (!handle) throw new IoxError(`Unable to create ${kind}`, 'invalid_argument');
      return handle;
    }));
}

function destroyReader(mod, handle) {
  if (handle) mod._native._iox_reader_destroy(handle);
}

function destroyWriter(mod, handle) {
  if (handle) mod._native._iox_writer_destroy(handle);
}

function readerFeed(mod, handle, bytes) {
  const native = mod._native;
  const pointer = allocateBytes(native, bytes);
  try {
    const status = native._iox_reader_feed(handle, pointer, bytes.length);
    if (status < 0) throw new IoxError('Reader feed failed', 'internal_error');
  } finally {
    if (pointer) native._iox_free(pointer);
  }
}

function readerFinish(mod, handle) {
  const status = mod._native._iox_reader_finish(handle);
  if (status < 0) throw new IoxError('Reader finish failed', 'internal_error');
}

function drainReader(mod, handle, diagnostics, allowEnd = false) {
  const native = mod._native;
  const events = [];
  while (true) {
    const result = callResult(mod, (out) => native._iox_reader_next(handle, out));
    addDiagnostics(diagnostics, result);
    if (result.status === -1 || result.status === -2 || result.status === -3) {
      throwResultError(result);
    }
    if (result.status === 1) {
      const event = resultEvent(result);
      if (event) events.push(event);
      continue;
    }
    if (result.status === 2) return events;
    if (result.status === 3) {
      if (!allowEnd) return events;
      return events;
    }
    throw new IoxError('Unexpected reader status', 'internal_error', diagnostics);
  }
}

function writerWrite(mod, handle, event) {
  const native = mod._native;
  if (!event || typeof event !== 'object' || !EVENT_TYPES.has(event.event)) {
    throw new TypeError('event must be an object with a supported event discriminator');
  }
  const serialized = textEncoder.encode(`${JSON.stringify(event)}\n`);
  const pointer = allocateBytes(native, serialized);
  try {
    const result = callResult(mod, (out) =>
      native._iox_writer_write_event_json(handle, pointer, serialized.length, out));
    if (result.status < 0) throwResultError(result);
  } finally {
    if (pointer) native._iox_free(pointer);
  }
}

function writerTakeOutput(mod, handle, diagnostics) {
  const result = callResult(mod, (out) => mod._native._iox_writer_take_output(handle, out));
  addDiagnostics(diagnostics, result);
  if (result.status < 0) throwResultError(result);
  return result.bytes;
}

/**
 * @typedef {Object} IoxModuleOptions
 * @property {(path: string) => string} [locateFile]
 */

/**
 * @typedef {Object} IoxModule
 * @property {() => number} abiVersion
 * @property {() => string} version
 */

/** Create an initialized iox-cpp WebAssembly module. */
export async function createIoxModule(options = {}) {
  const generated = new URL('./iox-wasm.mjs', import.meta.url);
  const { default: initialize } = await import(generated.href);
  const locateFile = options.locateFile ?? ((path) => new URL(path, generated).pathname);
  const native = await initialize({
    ...options,
    locateFile: (path) => {
      const located = locateFile(path);
      return located.startsWith('/') || located.includes(':')
        ? located
        : new URL(located, generated).pathname;
    },
  });
  return {
    abiVersion: () => native._iox_abi_version(),
    version: () => native.UTF8ToString(native._iox_version()),
    _native: native,
    _options: options,
  };
}

/** Read all XTF events from an input buffer. */
export function readAll(mod, input, options = {}) {
  const reader = new XtfReader(mod, input, options);
  try {
    return reader.readAll();
  } finally {
    reader.close();
  }
}

/** Write events to XTF and return the deterministic output bytes. */
export function writeAll(mod, events, options) {
  const writer = new XtfWriter(mod, options);
  try {
    for (const event of events) writer.write(event);
    return writer.finish();
  } finally {
    writer.close();
  }
}

/** Eager iterable XTF reader. */
export class XtfReader {
  constructor(mod, input, options = {}) {
    this._mod = mod;
    this._handle = 0;
    this._diagnostics = [];
    this._events = [];
    this._index = 0;
    this._closed = false;
    try {
      this._handle = createHandle(mod, 'reader', 'xtf', options);
      const bytes = inputBytes(input);
      const chunkSize = 64 * 1024;
      for (let offset = 0; offset < bytes.length; offset += chunkSize) {
        readerFeed(mod, this._handle, bytes.subarray(offset, offset + chunkSize));
        this._events.push(...drainReader(mod, this._handle, this._diagnostics));
      }
      if (bytes.length === 0) this._events.push(...drainReader(mod, this._handle, this._diagnostics));
      readerFinish(mod, this._handle);
      this._events.push(...drainReader(mod, this._handle, this._diagnostics, true));
    } catch (error) {
      this.close();
      throw error;
    }
  }

  [Symbol.iterator]() {
    return {
      next: () => {
        if (this._index >= this._events.length) return { done: true, value: undefined };
        return { done: false, value: this._events[this._index++] };
      },
      return: () => {
        this.close();
        return { done: true, value: undefined };
      },
    };
  }

  readAll() { return this._events.slice(); }
  diagnostics() { return this._diagnostics.slice(); }
  close() {
    if (!this._closed) {
      destroyReader(this._mod, this._handle);
      this._handle = 0;
      this._closed = true;
    }
  }
}

/** Incremental XTF reader. */
export class IncrementalXtfReader {
  constructor(mod, options = {}) {
    this._mod = mod;
    this._handle = createHandle(mod, 'reader', 'xtf', options);
    this._diagnostics = [];
    this._finished = false;
    this._closed = false;
  }

  feed(chunk) {
    if (this._closed || this._finished) throw new IoxError('Reader is closed', 'invalid_state');
    const bytes = inputBytes(chunk);
    readerFeed(this._mod, this._handle, bytes);
    return drainReader(this._mod, this._handle, this._diagnostics);
  }

  finish() {
    if (this._closed) throw new IoxError('Reader is closed', 'invalid_state');
    if (this._finished) return [];
    readerFinish(this._mod, this._handle);
    this._finished = true;
    return drainReader(this._mod, this._handle, this._diagnostics, true);
  }

  diagnostics() { return this._diagnostics.slice(); }
  close() {
    if (!this._closed) {
      destroyReader(this._mod, this._handle);
      this._handle = 0;
      this._closed = true;
    }
  }
}

/** Streaming XTF writer. */
export class XtfWriter {
  constructor(mod, options) {
    if (!options || (options.version !== '2.3' && options.version !== '2.4')) {
      throw new TypeError('XtfWriter requires version "2.3" or "2.4"');
    }
    this._mod = mod;
    this._handle = createHandle(mod, 'writer', options.version === '2.3' ? 'xtf23' : 'xtf24', options);
    this._diagnostics = [];
    this._chunks = [];
    this._finished = false;
    this._closed = false;
  }

  write(event) {
    if (this._closed || this._finished) throw new IoxError('Writer is closed', 'invalid_state');
    writerWrite(this._mod, this._handle, event);
    this._chunks.push(writerTakeOutput(this._mod, this._handle, this._diagnostics));
  }

  finish() {
    if (this._closed) throw new IoxError('Writer is closed', 'invalid_state');
    if (this._finished) return this._joinChunks();
    const result = callResult(this._mod, (out) =>
      this._mod._native._iox_writer_finish(this._handle, out));
    addDiagnostics(this._diagnostics, result);
    if (result.status < 0) throwResultError(result);
    this._chunks.push(result.bytes);
    this._finished = true;
    return this._joinChunks();
  }

  _joinChunks() {
    const total = this._chunks.reduce((sum, chunk) => sum + chunk.length, 0);
    const output = new Uint8Array(total);
    let offset = 0;
    for (const chunk of this._chunks) {
      output.set(chunk, offset);
      offset += chunk.length;
    }
    return output;
  }

  diagnostics() { return this._diagnostics.slice(); }
  close() {
    if (!this._closed) {
      destroyWriter(this._mod, this._handle);
      this._handle = 0;
      this._closed = true;
    }
  }
}
