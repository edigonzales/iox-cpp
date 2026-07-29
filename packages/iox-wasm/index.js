/**
 * @interlis/iox-wasm — INTERLIS XTF 2.3/2.4 Reader/Writer
 */

/**
 * @typedef {Object} IoxModuleOptions
 * @property {function(string): string} [locateFile]
 */

/**
 * @typedef {Object} IoxModule
 * @property {function(): number} abiVersion
 * @property {function(): string} version
 */

/**
 * @typedef {'StartTransfer'|'StartBasket'|'Object'|'EndBasket'|'EndTransfer'} IoxEventType
 */

/**
 * @typedef {Object} StartTransferEvent
 * @property {'StartTransfer'} type
 * @property {string} [sender]
 * @property {string} [comment]
 * @property {number} [version]
 */

/**
 * @typedef {Object} StartBasketEvent
 * @property {'StartBasket'} type
 * @property {string} bid
 * @property {string} [basketType]
 */

/**
 * @typedef {Object} ObjectEvent
 * @property {'Object'} type
 * @property {string} objectId
 * @property {string} [operation]
 * @property {Object} [object]
 */

/**
 * @typedef {Object} EndBasketEvent
 * @property {'EndBasket'} type
 * @property {string} bid
 */

/**
 * @typedef {Object} EndTransferEvent
 * @property {'EndTransfer'} type
 */

/**
 * @typedef {StartTransferEvent|StartBasketEvent|ObjectEvent|EndBasketEvent|EndTransferEvent} IoxEvent
 */

/**
 * @typedef {Object} XtfReaderOptions
 * @property {boolean} [strict=false]
 * @property {string} [sourceName]
 * @property {'2.3'|'2.4'} [expectedVersion]
 */

/**
 * @typedef {Object} XtfWriterOptions
 * @property {'2.3'|'2.4'} version
 * @property {boolean} [strict=false]
 * @property {boolean} [pretty=true]
 * @property {string} [sender]
 */

/**
 * Create an initialized iox-cpp WebAssembly module.
 * @param {IoxModuleOptions} [options]
 * @returns {Promise<IoxModule>}
 */
export async function createIoxModule(options = {}) {
  const generated = new URL('./iox-wasm.mjs', import.meta.url);
  const { default: initialize } = await import(generated.href);
  const locateFile = options.locateFile ?? ((path) =>
    new URL(path, generated).pathname);
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

/**
 * Read all XTF events from an input buffer.
 * @param {IoxModule} mod
 * @param {Uint8Array|ArrayBuffer|string} input
 * @param {XtfReaderOptions} [options]
 * @returns {IoxEvent[]}
 */
export function readAll(mod, input, options = {}) {
  // Stub: returns empty array until WASM C-ABI is linked
  return [];
}

/**
 * Write events to XTF and return the bytes.
 * @param {IoxModule} mod
 * @param {Iterable<IoxEvent>} events
 * @param {XtfWriterOptions} options
 * @returns {Uint8Array}
 */
export function writeAll(mod, events, options) {
  // Stub: returns empty buffer until WASM C-ABI is linked
  return new Uint8Array(0);
}

/**
 * XTF Reader class (iterable).
 */
export class XtfReader {
  /**
   * @param {IoxModule} mod
   * @param {Uint8Array|ArrayBuffer|string} input
   * @param {XtfReaderOptions} [options]
   */
  constructor(mod, input, options = {}) {
    this._mod = mod;
    this._input = input;
    this._options = options;
    this._events = [];
    this._index = 0;
  }

  /** @returns {Iterator<IoxEvent>} */
  [Symbol.iterator]() {
    return {
      next: () => {
        if (this._index >= this._events.length) {
          return { done: true, value: undefined };
        }
        return { done: false, value: this._events[this._index++] };
      }
    };
  }

  /** @returns {IoxEvent[]} */
  readAll() { return this._events; }

  /** @returns {Object[]} */
  diagnostics() { return []; }

  close() { this._events = []; this._index = 0; }
}

/**
 * Incremental XTF Reader.
 */
export class IncrementalXtfReader {
  /**
   * @param {IoxModule} mod
   * @param {XtfReaderOptions} [options]
   */
  constructor(mod, options = {}) {
    this._mod = mod;
    this._options = options;
  }

  /** @param {Uint8Array} chunk @returns {IoxEvent[]} */
  feed(chunk) { return []; }

  /** @returns {IoxEvent[]} */
  finish() { return []; }

  /** @returns {Object[]} */
  diagnostics() { return []; }

  close() {}
}

/**
 * XTF Writer.
 */
export class XtfWriter {
  /**
   * @param {IoxModule} mod
   * @param {XtfWriterOptions} options
   */
  constructor(mod, options) {
    this._mod = mod;
    this._options = options;
  }

  /** @param {IoxEvent} event */
  write(event) {}

  /** @returns {Uint8Array} */
  finish() { return new Uint8Array(0); }

  /** @returns {Object[]} */
  diagnostics() { return []; }

  close() {}
}
