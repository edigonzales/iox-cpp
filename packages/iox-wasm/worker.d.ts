import type {
  Diagnostic,
  IoxEvent,
  IoxModuleOptions,
  XtfReaderOptions,
  XtfWriterOptions,
} from './index.js';

export type WorkerRequest =
  | { type: 'init'; requestId: string | number; options?: IoxModuleOptions }
  | { type: 'readAll'; requestId: string | number;
      input: Uint8Array | ArrayBuffer | string; options?: XtfReaderOptions }
  | { type: 'writeAll'; requestId: string | number;
      events: IoxEvent[]; options: XtfWriterOptions }
  | { type: 'readerCreate'; requestId: string | number;
      streamId: string | number; options?: XtfReaderOptions }
  | { type: 'readerFeed'; requestId: string | number;
      streamId: string | number; input: Uint8Array | ArrayBuffer | string }
  | { type: 'readerFinish'; requestId: string | number;
      streamId: string | number }
  | { type: 'readerClose'; requestId: string | number;
      streamId: string | number }
  | { type: 'writerCreate'; requestId: string | number;
      streamId: string | number; options: XtfWriterOptions }
  | { type: 'writerWrite'; requestId: string | number;
      streamId: string | number; event: IoxEvent }
  | { type: 'writerFinish'; requestId: string | number;
      streamId: string | number }
  | { type: 'writerClose'; requestId: string | number;
      streamId: string | number }
  | { type: 'close'; requestId: string | number };

export interface WorkerSuccess {
  requestId: string | number;
  ok: true;
  events?: IoxEvent[];
  bytes?: ArrayBuffer;
  diagnostics?: Diagnostic[];
  abiVersion?: number;
  version?: string;
}

export interface WorkerFailure {
  requestId: string | number;
  ok: false;
  error: { code: string; message: string; diagnostics?: Diagnostic[] };
}

export type WorkerResponse = WorkerSuccess | WorkerFailure;

export function handleWorkerMessage(
  request: WorkerRequest,
  postMessage: (message: WorkerResponse, transfer?: ArrayBuffer[]) => void,
): Promise<void>;
