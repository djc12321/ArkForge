import { webview } from '@kit.ArkWeb';

class MonacoBridge {
  ready: () => void = () => {};
  changed: (text: string) => void = () => {};
  selected: (line: number) => void = () => {};
  saveContent: (text: string) => void = () => {};
  onReady(): void { this.ready(); }
}

@Component
export struct MonacoEditor {
  @Prop @Watch('sync') code: string = '';
  @Prop @Watch('sync') activeName: string = '';
  @Prop themeId: string = '';
  @Prop @Watch('reveal') revealToken: string = '';
  onContentChange: (text: string) => void = () => {};
  onSave: () => void = () => {};
  onLineSelected: (line: number) => void = () => {};
  private controller: webview.WebviewController = new webview.WebviewController();
  private bridge: MonacoBridge = new MonacoBridge();
  private loaded: boolean = false;

  aboutToAppear(): void {
    this.bridge.ready = () => { this.loaded = true; this.sync(); };
    this.bridge.changed = (text: string) => { if (text !== this.code) this.onContentChange(text); this.onSave(); };
    this.bridge.saveContent = (text: string) => { this.onContentChange(text); this.onSave(); };
    this.bridge.selected = (line: number) => { this.onLineSelected(line); };
  }
  private sync(): void {
    if (!this.loaded) return;
    this.controller.runJavaScript('setEditorContent(' + JSON.stringify(this.code) + ',' +
      JSON.stringify(this.activeName) + ')').catch(() => {});
  }
  private reveal(): void {
    const line = Number(this.revealToken.split(':')[0]);
    if (this.loaded && line > 0) this.controller.runJavaScript('revealLine(' + line + ')').catch(() => {});
  }
  build() {
    Web({ src: $rawfile('editor/index.html'), controller: this.controller })
      .javaScriptAccess(true).fileAccess(true)
      .javaScriptProxy({ object: this.bridge, name: 'editorBridge',
        methodList: ['onReady', 'changed', 'selected', 'saveContent'], controller: this.controller })
      .width('100%').height('100%').backgroundColor('#1e1e1e')
  }
}
