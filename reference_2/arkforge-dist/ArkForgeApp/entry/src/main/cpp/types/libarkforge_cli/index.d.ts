export interface CommandResult { output: string; code: number; waitCode: number; traceCode: number; }
export interface TerminalSession { id: number; pid: number; }
export interface TerminalOutput { data: string; code: number; closed: boolean; }
declare const cli: {
  run(cwd: string, home: string, args: string[], credential?: string): Promise<CommandResult>;
  terminalOpen(cwd: string, home: string, credential: string, cols: number, rows: number): TerminalSession;
  terminalRead(id: number): Promise<TerminalOutput>;
  terminalWrite(id: number, data: string): Promise<TerminalOutput>;
  terminalResize(id: number, cols: number, rows: number): void;
  terminalClose(id: number): void;
};
export default cli;
