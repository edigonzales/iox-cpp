/**
 * Worker protocol for @ilic/iox-wasm.
 *
 * Use this file as a module worker. The exported handler also provides a
 * browser-independent harness for Node tests.
 *
 * Requests: init, readAll/writeAll, streaming reader/writer operations, close.
 * Responses always contain the caller's requestId and an `ok` discriminator.
 */

import {
  createIoxModule,
  IncrementalXtfReader,
  XtfWriter,
  readAll,
  writeAll,
} from './index.js';

let modulePromise = null;
const readers = new Map();
const writers = new Map();

function errorPayload(error) {
  return {
    code: error?.code ?? (error instanceof TypeError
      ? 'api.invalid_argument' : 'internal.error'),
    message: error?.message ?? String(error),
    diagnostics: error?.diagnostics ?? [],
  };
}

/**
 * Handle one protocol request and send exactly one response.
 * @param {object} request
 * @param {(message: object, transfer?: Transferable[]) => void} postMessage
 */
export async function handleWorkerMessage(request, postMessage) {
  const reply = (message, transfer = []) => postMessage({
    requestId: request?.requestId,
    ...message,
  }, transfer);

  try {
    const module = async () => {
      modulePromise ??= createIoxModule();
      return modulePromise;
    };
    const streamId = () => {
      if (request?.streamId === undefined || request?.streamId === null) {
        throw Object.assign(new Error('streamId is required'), {
          code: 'api.invalid_argument',
        });
      }
      return request.streamId;
    };
    const transferBytes = (bytes, extra = {}) => {
      const buffer = bytes.buffer.slice(
        bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      reply({ ok: true, ...extra, bytes: buffer }, [buffer]);
    };
    switch (request?.type) {
    case 'init': {
      modulePromise ??= createIoxModule(request.options ?? {});
      const module = await modulePromise;
      reply({ ok: true, abiVersion: module.abiVersion(), version: module.version() });
      return;
    }
    case 'readAll': {
      const events = readAll(await module(), request.input,
        request.options ?? {});
      reply({ ok: true, events });
      return;
    }
    case 'writeAll': {
      const bytes = writeAll(await module(), request.events ?? [],
        request.options);
      transferBytes(bytes);
      return;
    }
    case 'readerCreate': {
      const id = streamId();
      if (readers.has(id)) {
        throw Object.assign(new Error('Reader stream already exists'), {
          code: 'api.invalid_state',
        });
      }
      readers.set(id, new IncrementalXtfReader(
        await module(), request.options ?? {}));
      reply({ ok: true });
      return;
    }
    case 'readerFeed': {
      const id = streamId();
      const reader = readers.get(id);
      if (!reader) {
        throw Object.assign(new Error('Unknown reader stream'), {
          code: 'api.invalid_state',
        });
      }
      try {
        const events = reader.feed(request.input);
        reply({ ok: true, events, diagnostics: reader.diagnostics() });
      } catch (error) {
        reader.close();
        readers.delete(id);
        throw error;
      }
      return;
    }
    case 'readerFinish': {
      const id = streamId();
      const reader = readers.get(id);
      if (!reader) {
        throw Object.assign(new Error('Unknown reader stream'), {
          code: 'api.invalid_state',
        });
      }
      try {
        const events = reader.finish();
        reply({ ok: true, events, diagnostics: reader.diagnostics() });
      } finally {
        reader.close();
        readers.delete(id);
      }
      return;
    }
    case 'readerClose': {
      const id = streamId();
      const reader = readers.get(id);
      if (!reader) {
        throw Object.assign(new Error('Unknown reader stream'), {
          code: 'api.invalid_state',
        });
      }
      reader.close();
      readers.delete(id);
      reply({ ok: true });
      return;
    }
    case 'writerCreate': {
      const id = streamId();
      if (writers.has(id)) {
        throw Object.assign(new Error('Writer stream already exists'), {
          code: 'api.invalid_state',
        });
      }
      writers.set(id, new XtfWriter(await module(), request.options));
      reply({ ok: true });
      return;
    }
    case 'writerWrite': {
      const id = streamId();
      const writer = writers.get(id);
      if (!writer) {
        throw Object.assign(new Error('Unknown writer stream'), {
          code: 'api.invalid_state',
        });
      }
      try {
        writer.write(request.event);
        const bytes = writer.takeOutput();
        transferBytes(bytes, { diagnostics: writer.diagnostics() });
      } catch (error) {
        writer.close();
        writers.delete(id);
        throw error;
      }
      return;
    }
    case 'writerFinish': {
      const id = streamId();
      const writer = writers.get(id);
      if (!writer) {
        throw Object.assign(new Error('Unknown writer stream'), {
          code: 'api.invalid_state',
        });
      }
      try {
        const bytes = writer.finish();
        transferBytes(bytes, { diagnostics: writer.diagnostics() });
      } finally {
        writer.close();
        writers.delete(id);
      }
      return;
    }
    case 'writerClose': {
      const id = streamId();
      const writer = writers.get(id);
      if (!writer) {
        throw Object.assign(new Error('Unknown writer stream'), {
          code: 'api.invalid_state',
        });
      }
      writer.close();
      writers.delete(id);
      reply({ ok: true });
      return;
    }
    case 'close':
      for (const reader of readers.values()) reader.close();
      for (const writer of writers.values()) writer.close();
      readers.clear();
      writers.clear();
      modulePromise = null;
      reply({ ok: true });
      return;
    default:
      reply({ ok: false, error: {
        code: 'api.invalid_argument',
        message: `Unknown type: ${request?.type}`,
      } });
    }
  } catch (error) {
    reply({ ok: false, error: errorPayload(error) });
  }
}

if (typeof self !== 'undefined' && typeof self.postMessage === 'function') {
  self.onmessage = (event) => handleWorkerMessage(
    event.data,
    (message, transfer) => self.postMessage(message, transfer)
  );
} else if (typeof process === 'object' && process?.versions?.node) {
  const { parentPort } = await import('node:worker_threads');
  parentPort?.on('message', (request) => handleWorkerMessage(
    request,
    (message, transfer) => parentPort.postMessage(message, transfer)
  ));
}
