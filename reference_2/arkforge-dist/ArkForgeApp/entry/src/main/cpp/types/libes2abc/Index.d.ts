export interface CompileResult {
  ok: boolean;
  state: number;
  output: string;
  error: string;
}
export const compileArkTS: (input: string, output: string, resourceAnchor: string) => CompileResult;
