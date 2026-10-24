import { WorkbenchRuntime } from '../../services/WorkbenchRuntime';
import { webview } from '@kit.ArkWeb';
import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { zlib } from '@kit.BasicServicesKit';
import resourceTool from 'libarkforge_resources.so';
import { AtomicFiles } from '../../services/AtomicFiles';
import { AgentModel } from '../../services/AgentModel';

interface WorkbenchManifest { sha256: string; commit: string; }
class WorkbenchBridge {
  dispatch: (id: number, method: string, payload: string) => void = () => {};
  request(id: number, method: string, payload: string): void { this.dispatch(id,method,payload); }
}

@Component
export struct CodeOSSWorkbench {
  onTool: (method: string, payload: string) => Promise<string> = async () => { throw new Error('工程未打开'); };
  @Prop @Watch('appendBuildLog') buildEvent: string = '';
  @State private url: string = '';
  @State private status: string = '正在准备 ArkForge 工作台…';
  private controller: webview.WebviewController = new webview.WebviewController();
  private bridge: WorkbenchBridge = new WorkbenchBridge();
  private console: string[] = [];
  private bridgeTrace: string[] = [];
  private trace(message: string): void {
    this.bridgeTrace = this.bridgeTrace.slice(-63).concat(new Date().toISOString() + ' ' + message);
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    AtomicFiles.write(context.filesDir + '/codeoss-bridge.log',this.bridgeTrace.join('\n'));
  }

  aboutToDisappear(): void { WorkbenchRuntime.bind(); }
  aboutToAppear(): void {
    WorkbenchRuntime.bind(this.controller);
    this.bridge.dispatch = (id: number,method: string,payload: string) => { this.execute(id,method,payload); };
    this.prepare().catch((error: Error) => { this.status = '工作台启动失败：' + error.message; });
  }
  private async prepare(): Promise<void> {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    const metadata = await context.resourceManager.getRawFileContent('codeoss-manifest.json');
    const manifest = JSON.parse(util.TextDecoder.create('utf-8').decodeToString(metadata)) as WorkbenchManifest;
    const root = context.filesDir + '/codeoss/' + manifest.sha256;
    if (!fileIo.accessSync(root + '/.ready')) {
      fileIo.mkdirSync(root,true);
      const archive = root + '.zip';
      AtomicFiles.writeBytes(archive,await context.resourceManager.getRawFileContent('codeoss.zip'));
      resourceTool.validateArchive(archive);
      await zlib.decompressFile(archive,root,{});
      if (!fileIo.accessSync(root + '/out/vs/workbench/workbench.web.main.internal.js')) throw new Error('ArkForge 工作台资源不完整');
      AtomicFiles.write(root + '/.ready',manifest.commit);
    }
    const port = resourceTool.startWorkbench(root);
    this.url = 'http://127.0.0.1:' + port + '/index.html';
  }
  private async execute(id: number,method: string,payload: string): Promise<void> {
    if (method !== 'terminal_read' && method !== 'terminal_write') this.trace(method + ' START');
    let result = ''; let error = '';
    try {
      if (method === 'cancel_model') { AgentModel.cancel(); result = '模型请求已停止'; }
      else {
        const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
        if (method === 'agent_config_get') result = await AgentModel.configuration(context);
        else if (method === 'agent_config_save') result = await AgentModel.configure(context,payload);
        else result = method === 'chat' ? await AgentModel.request(context,payload) : await this.onTool(method,payload);
      }
    }
    catch (cause) { error = String(cause); }
    if (method !== 'terminal_read' && method !== 'terminal_write') this.trace(method + ' DONE chars=' + result.length + (error ? ' ERROR' : ''));
    try {
      if (!error && result.length > 32768) {
        for (let offset = 0; offset < result.length; offset += 32768) {
          await this.controller.runJavaScript('nativeReplyChunk(' + id + ',' + JSON.stringify(result.slice(offset,offset + 32768)) + ',' + (offset + 32768 >= result.length) + ')');
        }
      } else {
        await this.controller.runJavaScript('nativeReply(' + id + ',' + JSON.stringify(result) + ',' + JSON.stringify(error) + ')');
      }
    } catch (_) {
      this.trace(method + ' TRANSFER_FAILED');
      this.controller.runJavaScript('nativeReply(' + id + ',"","本机结果传输失败，请重试或重新打开工作区")').catch(() => {});
    }
  }
  private appendBuildLog(): void {
    if (this.url && this.buildEvent) this.controller.runJavaScript('window.arkforgeBuildEvent?.(' + JSON.stringify(this.buildEvent) + ')').catch(() => {});
  }
  build() {
    Column() {
      if (this.url) {
        Web({src:this.url,controller:this.controller})
          .javaScriptAccess(true).domStorageAccess(true).fileAccess(false)
          .javaScriptProxy({object:this.bridge,name:'arkforgeNative',methodList:['request'],controller:this.controller})
          .onErrorReceive((event) => {
            if (event) {
              const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
              AtomicFiles.write(context.filesDir + '/codeoss-network-error.log',event.request.getRequestUrl() + '\n' + event.error.getErrorInfo());
            }
          })
          .onConsole((event) => {
            if (event) {
              this.console = this.console.slice(-79).concat(event.message.getMessage());
              const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
              AtomicFiles.write(context.filesDir + '/codeoss-console.log',this.console.join('\n'));
            }
            return true;
          })
          .width('100%').height('100%')
      } else { Text(this.status).fontSize(15).fontColor('#cccccc').padding(24) }
    }.width('100%').height('100%').backgroundColor('#1f1f1f')
  }
}
