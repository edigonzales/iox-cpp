/**
 * Worker protocol for @interlis/iox-wasm.
 *
 * Use this file as a module worker. The exported handler also provides a
 * browser-independent harness for Node tests.
 *
 * Requests: init, readAll, writeAll, close.
 * Responses always contain the caller's requestId and an `ok` discriminator.
 */

import { createIoxModule, readAll, writeAll } from './index.js';

let modulePromise = null;

function errorPayload(error) {
  return {
    code: error?.code ?? 'internal_error',
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
    switch (request?.type) {
    case 'init': {
      modulePromise ??= createIoxModule(request.options ?? {});
      const module = await modulePromise;
      reply({ ok: true, abiVersion: module.abiVersion(), version: module.version() });
      return;
    }
    case 'readAll': {
      if (!modulePromise) modulePromise = createIoxModule();
      const module = await modulePromise;
      const events = readAll(module, request.input, request.options ?? {});
      reply({ ok: true, events });
      return;
    }
    case 'writeAll': {
      if (!modulePromise) modulePromise = createIoxModule();
      const module = await modulePromise;
      const bytes = writeAll(module, request.events ?? [], request.options);
      reply({ ok: true, bytes: bytes.buffer }, [bytes.buffer]);
      return;
    }
    case 'close':
      modulePromise = null;
      reply({ ok: true });
      return;
    default:
      reply({ ok: false, error: {
        code: 'invalid_argument',
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
}
