/**
 * TypeScript declarations for @interlis/iox-wasm
 * Phase 0 stub — will be expanded in Phase 8.
 */

export interface IoxModuleOptions {
  locateFile?: (path: string) => string;
}

export interface IoxModule {
  abiVersion(): number;
  version(): string;
}

export function createIoxModule(options?: IoxModuleOptions): Promise<IoxModule>;
