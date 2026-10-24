import { webview } from '@kit.ArkWeb';
import { fileIo } from '@kit.CoreFileKit';

class SDKResultBridge {
  complete(id: string, text: string): void { SDKWebRuntime.complete(id, text); }
}

// The renderer runs the unchanged official SDK against an offline virtual filesystem.
// A fresh document releases the entire checker graph between builds.
export class SDKWebRuntime {
  static controller: webview.WebviewController = new webview.WebviewController();
  static bridge: SDKResultBridge = new SDKResultBridge();
  private static pending: ((text: string) => void) | undefined = undefined;
  private static timer: number = -1;
  private static active: string = '';
  private static sequence: number = 0;
  private static reporter: ((text: string) => void) | undefined = undefined;

  static log(text: string): void {
    if (SDKWebRuntime.reporter) SDKWebRuntime.reporter(text);
  }

  static complete(id: string, text: string): void {
    if (!SDKWebRuntime.pending || id !== SDKWebRuntime.active) return;
    clearTimeout(SDKWebRuntime.timer);
    const resolve = SDKWebRuntime.pending;
    SDKWebRuntime.pending = undefined;
    SDKWebRuntime.reporter = undefined;
    resolve(text);
    SDKWebRuntime.controller.loadUrl('about:blank');
  }

  private static write(path: string, text: string): void {
    const file = fileIo.openSync(path, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, text); } finally { fileIo.closeSync(file); }
  }

  static check(bundle: string, input: string, report: (text: string) => void): Promise<string> {
    return SDKWebRuntime.run(bundle, 'var __arkforgeOptions=' + input + ';', 'sdk-check.web.js', '__arkforgeCheckResult', report);
  }

  static transform(bundle: string, options: string, names: string[], report: (text: string) => void): Promise<string> {
    return SDKWebRuntime.run(bundle, 'var __arkforgeOptions=' + options + ';var __arkforgeBatchFileNames=' + JSON.stringify(names) + ';',
      'ui-transform.web.js', '__arkforgeTransformResult', report);
  }

  private static run(bundle: string, inputScript: string, entryScript: string, resultName: string, report: (text: string) => void): Promise<string> {
    if (SDKWebRuntime.pending) return Promise.reject(new Error('SDK runtime is already running'));
    const directory = bundle.substring(0, bundle.lastIndexOf('/'));
    const id = String(++SDKWebRuntime.sequence);
    SDKWebRuntime.write(directory + '/input.js', inputScript);
    SDKWebRuntime.write(directory + '/check.html', '<!doctype html><meta charset="utf-8">' +
      '<meta http-equiv="Content-Security-Policy" content="default-src \'none\'; script-src \'self\' \'unsafe-inline\' \'unsafe-eval\'; connect-src \'none\'">' +
      '<script>window.onerror=function(message){SdkBridge.complete("' + id + '",JSON.stringify({ok:false,error:String(message)}));};</script>' +
      '<script src="input.js"></script><script src="' + entryScript + '"></script>' +
      '<script>SdkBridge.complete("' + id + '",globalThis.' + resultName + ' || JSON.stringify({ok:false,error:"Missing SDK result"}));</script>');
    return new Promise<string>((resolve) => {
      SDKWebRuntime.pending = resolve;
      SDKWebRuntime.active = id;
      SDKWebRuntime.reporter = report;
      SDKWebRuntime.timer = setTimeout(() => SDKWebRuntime.complete(id, JSON.stringify({ok:false,error:'Official SDK check timed out'})), 300000);
      SDKWebRuntime.controller.loadUrl('file://' + directory + '/check.html?run=' + Date.now());
    });
  }
}
