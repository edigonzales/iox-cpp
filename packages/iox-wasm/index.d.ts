/** TypeScript declarations for @interlis/iox-wasm. */

export interface IoxModuleOptions {
  locateFile?: (path: string) => string;
}

export interface IoxModule {
  abiVersion(): number;
  version(): string;
}

export type IomPrimitive = string | number | boolean | null;

export interface IomAttribute {
  name: string;
  value?: IomPrimitive | IomObject;
  values?: Array<IomPrimitive | IomObject>;
  ref?: string;
  bid?: string;
  orderPos?: string | number;
}

export interface IomObject {
  tag: string;
  attrs?: IomAttribute[];
  ref?: string;
  bid?: string;
  orderPos?: string | number;
}

export interface StartTransferEvent {
  event: 'startTransfer';
  sender?: string;
  comment?: string;
  iliVersion?: string;
  software?: string;
  date?: string;
  version?: number;
}

export interface StartBasketEvent {
  event: 'startBasket';
  basketType: string;
  bid: string;
  consistency?: string;
  operation?: string;
  oidDomain?: number;
  startState?: string;
  endState?: string;
  kind?: string;
  domains?: string[];
}

export interface ObjectEvent {
  event: 'object';
  operation: string;
  objectId: string;
  consistency?: string;
  refBid?: string;
  refOrderPos?: string;
  object: IomObject;
}

export interface EndBasketEvent {
  event: 'endBasket';
  bid: string;
}

export interface EndTransferEvent {
  event: 'endTransfer';
}

export type IoxEvent =
  | StartTransferEvent
  | StartBasketEvent
  | ObjectEvent
  | EndBasketEvent
  | EndTransferEvent;

export interface DiagnosticLocation {
  sourceName?: string;
  byteOffset?: number;
  line?: number;
  column?: number;
}

export interface Diagnostic {
  severity?: 'Warning' | 'Error' | 'Fatal';
  code: string;
  message: string;
  location?: DiagnosticLocation;
}

export interface XtfReaderOptions {
  strict?: boolean;
  sourceName?: string;
  expectedVersion?: '2.3' | '2.4';
  preserveUnknownExtensions?: boolean;
}

export interface XtfWriterOptions {
  version: '2.3' | '2.4';
  strict?: boolean;
  pretty?: boolean;
  sender?: string;
  comment?: string;
  software?: string;
}

export class IoxError extends Error {
  readonly code: string;
  readonly diagnostics: Diagnostic[];
  constructor(message: string, code?: string, diagnostics?: Diagnostic[]);
}

export function createIoxModule(options?: IoxModuleOptions): Promise<IoxModule>;

export function readAll(
  module: IoxModule,
  input: Uint8Array | ArrayBuffer | string,
  options?: XtfReaderOptions
): IoxEvent[];

export function writeAll(
  module: IoxModule,
  events: Iterable<IoxEvent>,
  options: XtfWriterOptions
): Uint8Array;

export class XtfReader implements Iterable<IoxEvent> {
  constructor(module: IoxModule,
              input: Uint8Array | ArrayBuffer | string,
              options?: XtfReaderOptions);
  [Symbol.iterator](): Iterator<IoxEvent>;
  readAll(): IoxEvent[];
  diagnostics(): Diagnostic[];
  close(): void;
}

export class IncrementalXtfReader {
  constructor(module: IoxModule, options?: XtfReaderOptions);
  feed(chunk: Uint8Array | ArrayBuffer | string): IoxEvent[];
  finish(): IoxEvent[];
  diagnostics(): Diagnostic[];
  close(): void;
}

export class XtfWriter {
  constructor(module: IoxModule, options: XtfWriterOptions);
  write(event: IoxEvent): void;
  finish(): Uint8Array;
  diagnostics(): Diagnostic[];
  close(): void;
}
