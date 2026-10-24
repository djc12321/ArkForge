import { webview } from '@kit.ArkWeb';

/** Batch transport preserves every log row without rerendering ArkUI per row. */
export class WorkbenchRuntime {
  private static controller: webview.WebviewController | undefined = undefined;
  private static lines: string[] = [];
  private static timer: number = -1;
  static bind(controller?: webview.WebviewController): void { WorkbenchRuntime.controller = controller; }
  static append(line: string): void {
    WorkbenchRuntime.lines.push(line);
    if (WorkbenchRuntime.timer >= 0) return;
    WorkbenchRuntime.timer = setTimeout(() => {
      WorkbenchRuntime.timer = -1;
      const text = WorkbenchRuntime.lines.join('\n'); WorkbenchRuntime.lines = [];
      WorkbenchRuntime.controller?.runJavaScript('window.arkforgeBuildEvent?.(' + JSON.stringify(text) + ')').catch(() => {});
    },50);
  }
}
