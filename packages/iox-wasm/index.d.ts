/** TypeScript declarations for @interlis/iox-wasm 0.2. */

export interface IoxModuleOptions {
  locateFile?: (path: string) => string;
  wasmBinary?: Uint8Array;
}

export interface IoxModule {
  abiVersion(): number;
  version(): string;
}

export interface XmlQualifiedName {
  namespaceUri: string;
  localName: string;
  prefixHint: string;
}

export interface IomName {
  interlisName: string;
  xml: XmlQualifiedName | null;
}

export interface SourceLocation {
  sourceName: string;
  byteOffset: number;
  line: number;
  column: number;
}

export interface ReferenceInfo {
  targetOid?: string;
  targetBasketId?: string;
  orderPosition?: number;
}

export interface PrimitiveValue {
  kind: 'primitive';
  value: string;
}

export interface ObjectValue {
  kind: 'object';
  value: IomObject;
}

export type IomValue = PrimitiveValue | ObjectValue;

export interface IomAttribute {
  name: IomName;
  values: IomValue[];
}

export interface IomObject {
  tag: IomName;
  oid?: string;
  operation: 'insert' | 'update' | 'delete' | 'none';
  consistency: 'complete' | 'incomplete' | 'inconsistent' | 'adapted' | 'unspecified';
  reference: ReferenceInfo | null;
  location: SourceLocation;
  attributes: IomAttribute[];
}

export interface ExtensionAttribute {
  name: XmlQualifiedName;
  value: string;
}

export interface ExtensionElement {
  name: XmlQualifiedName;
  attributes: ExtensionAttribute[];
  text: string;
  children: ExtensionElement[];
}

export interface ModelEntry {
  name: string;
  version?: string;
  uri?: string;
  xmlNamespace: XmlQualifiedName;
}

export interface OidSpace {
  name: string;
  domain: string;
}

export interface TransferHeader {
  version: '2.3' | '2.4';
  sender: string;
  comment?: string;
  models: ModelEntry[];
  oidSpaces: OidSpace[];
  extensions: ExtensionElement[];
}

export interface BasketMetadata {
  topic: IomName;
  basketId: string;
  kind: 'full' | 'update' | 'initial' | 'unspecified';
  consistency: 'complete' | 'incomplete' | 'inconsistent' | 'adapted' | 'unspecified';
  startState?: string;
  endState?: string;
  domains: string[];
  topics: string[];
  extensions: ExtensionElement[];
  location: SourceLocation;
}

interface EventEnvelope {
  schema: 'iox-event/2';
}

export interface StartTransferEvent extends EventEnvelope {
  event: 'startTransfer';
  header: TransferHeader;
}

export interface StartBasketEvent extends EventEnvelope {
  event: 'startBasket';
  basket: BasketMetadata;
}

export interface ObjectEvent extends EventEnvelope {
  event: 'object';
  object: IomObject;
}

export interface EndBasketEvent extends EventEnvelope {
  event: 'endBasket';
}

export interface EndTransferEvent extends EventEnvelope {
  event: 'endTransfer';
}

export type IoxEvent = StartTransferEvent | StartBasketEvent | ObjectEvent |
  EndBasketEvent | EndTransferEvent;

export interface Diagnostic {
  severity: 'info' | 'warning' | 'error' | 'fatal';
  code: string;
  message: string;
  location: SourceLocation;
  contextPath: string[];
}

export interface XtfReaderOptions {
  strict?: boolean;
  sourceName?: string;
  expectedVersion?: '2.3' | '2.4';
  preserveUnknownExtensions?: boolean;
  requireAtLeastOneModel?: boolean;
  allowVersionAutoDetection?: boolean;
  maxDepth?: number;
  maxAttributesPerElement?: number;
  maxTextBytesPerNode?: number;
  maxTotalInputBytes?: number;
  maxQueuedEvents?: number;
}

export interface XtfWriterOptions {
  version: '2.3' | '2.4';
  strict?: boolean;
  pretty?: boolean;
  sender?: string;
  comment?: string;
  software?: string;
  preserveUnknownExtensions?: boolean;
  deterministicPrefixes?: boolean;
}

export class IoxError extends Error {
  readonly code: string;
  readonly diagnostics: Diagnostic[];
  constructor(message: string, code?: string, diagnostics?: Diagnostic[]);
}

export function createIoxModule(options?: IoxModuleOptions): Promise<IoxModule>;
export function readAll(module: IoxModule,
                        input: Uint8Array | ArrayBuffer | string,
                        options?: XtfReaderOptions): IoxEvent[];
export function writeAll(module: IoxModule, events: Iterable<IoxEvent>,
                         options: XtfWriterOptions): Uint8Array;

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
  takeOutput(): Uint8Array;
  finish(): Uint8Array;
  diagnostics(): Diagnostic[];
  close(): void;
}
