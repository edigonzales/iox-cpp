/**
 * @interlis/iox-wasm Worker protocol
 *
 * Phase 0 stub. Full message protocol in Phase 8.
 *
 * Messages:
 *   { type: "init" }
 *   { type: "close" }
 */

// Worker stub: listens for init and responds with version.
self.onmessage = async (e) => {
  const { type, requestId } = e.data;
  switch (type) {
    case "init":
      self.postMessage({ requestId, ok: true, abiVersion: 1, version: "0.1.0" });
      break;
    case "close":
      self.postMessage({ requestId, ok: true });
      break;
    default:
      self.postMessage({ requestId, ok: false, error: `Unknown type: ${type}` });
  }
};
