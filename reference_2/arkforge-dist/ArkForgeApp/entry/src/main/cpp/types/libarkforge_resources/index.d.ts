export const compile: (args: string[]) => number;
export const validateArchive: (path: string) => void;

export const startWorkbench: (root: string) => number;
export const startHuaweiCallback: (state: string) => number;
export const takeHuaweiCallback: () => string;
export const huaweiCallbackDiagnostics: () => string;
export const cancelHuaweiCallback: () => void;
