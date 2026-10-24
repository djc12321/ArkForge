import { webview } from '@kit.ArkWeb';
import { fileIo } from '@kit.CoreFileKit';

class SDKResultBridge {
  private session: SDKWebSession;
  constructor(session: SDKWebSession) { this.session = session; }
  complete(id: string, text: string): void { this.session.complete(id, text); }
}

/** Each compiler task owns its renderer, input files, bridge and lifetime. */
class SDKWebSession {
  controller: webview.WebviewController = new webview.WebviewController();
  bridge: SDKResultBridge = new SDKResultBridge(this);
  private pending: ((text: string) => void) | undefined = undefined;
  private timer: number = -1;
  private active: string = '';
  private sequence: number = 0;
  private reporter: ((text: string) => void) | undefined = undefined;

  log(text: string): void { if (this.reporter) this.reporter(text); }

  complete(id: string, text: string): void {
    if (!this.pending || id !== this.active) return;
    clearTimeout(this.timer);
    const resolve = this.pending;
    this.pending = undefined;
    this.reporter = undefined;
    resolve(text);
    this.controller.loadUrl('about:blank');
  }

  cancel(): void { this.complete(this.active, JSON.stringify({ok:false,error:'Compiler task canceled'})); }

  private write(path: string, text: string): void {
    const file = fileIo.openSync(path, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, text); } finally { fileIo.closeSync(file); }
  }

  run(bundle: string, inputScript: string, entryScript: string, resultName: string, report: (text: string) => void): Promise<string> {
    if (this.pending) return Promise.reject(new Error('SDK runtime is already running'));
    const directory = bundle.substring(0, bundle.lastIndexOf('/'));
    const id = String(++this.sequence);
    this.write(directory + '/input.js', inputScript);
    this.write(directory + '/check.html', '<!doctype html><meta charset="utf-8">' +
      '<meta http-equiv="Content-Security-Policy" content="default-src \'none\'; script-src \'self\' \'unsafe-inline\' \'unsafe-eval\'; connect-src \'none\'">' +
      '<script>window.onerror=function(message){SdkBridge.complete("' + id + '",JSON.stringify({ok:false,error:String(message)}));};</script>' +
      '<script src="input.js"></script><script src="' + entryScript + '"></script>' +
      '<script>SdkBridge.complete("' + id + '",globalThis.' + resultName + ' || JSON.stringify({ok:false,error:"Missing SDK result"}));</script>');
    return new Promise<string>((resolve) => {
      this.pending = resolve;
      this.active = id;
      this.reporter = report;
      this.timer = setTimeout(() => this.complete(id, JSON.stringify({ok:false,error:'Official SDK check timed out'})), 300000);
      try { this.controller.loadUrl('file://' + directory + '/check.html?run=' + Date.now()); }
      catch (error) { this.complete(id, JSON.stringify({ok:false,error:String(error)})); }
    });
  }
}

export class SDKWebRuntime {
  private static createSession(): SDKWebSession {
    webview.WebviewController.setRenderProcessMode(webview.RenderProcessMode.MULTIPLE);
    return new SDKWebSession();
  }
  private static checker: SDKWebSession = SDKWebRuntime.createSession();
  private static transformer: SDKWebSession = SDKWebRuntime.createSession();
  static controller: webview.WebviewController = SDKWebRuntime.checker.controller;
  static bridge: SDKResultBridge = SDKWebRuntime.checker.bridge;
  static transformController: webview.WebviewController = SDKWebRuntime.transformer.controller;
  static transformBridge: SDKResultBridge = SDKWebRuntime.transformer.bridge;
  static log(text: string): void { SDKWebRuntime.checker.log(text); }
  static transformLog(text: string): void { SDKWebRuntime.transformer.log(text); }
  static complete(id: string, text: string): void { SDKWebRuntime.checker.complete(id, text); }
  static cancelTransform(): void { SDKWebRuntime.transformer.cancel(); }

  static check(bundle: string, input: string, report: (text: string) => void): Promise<string> {
    return SDKWebRuntime.checker.run(bundle, 'var __arkforgeOptions=' + input + ';', 'sdk-check.web.js', '__arkforgeCheckResult', report);
  }

  static transform(bundle: string, options: string, names: string[], report: (text: string) => void): Promise<string> {
    return SDKWebRuntime.transformer.run(bundle, 'var __arkforgeOptions=' + options + ';var __arkforgeBatchFileNames=' + JSON.stringify(names) + ';',
      'ui-transform.web.js', '__arkforgeTransformResult', report);
  }
}
