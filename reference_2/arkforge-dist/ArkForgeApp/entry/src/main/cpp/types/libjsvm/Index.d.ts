export const evalScript: (code: string) => string;
export const evalCompiler: (code: string) => string;
export const resetVM: () => void;
export const inspectLibrary: (name: string) => string;
export const startToolLog: (path: string) => void;
export const stopToolLog: () => void;
