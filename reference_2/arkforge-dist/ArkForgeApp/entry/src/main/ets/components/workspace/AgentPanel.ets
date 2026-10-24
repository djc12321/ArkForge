import { webview } from '@kit.ArkWeb';
import { http } from '@kit.NetworkKit';

interface ChatRequest { base: string; key: string; body: string; }
class AgentBridge {
  dispatch: (id: number, method: string, payload: string) => void = () => {};
  request(id: number, method: string, payload: string): void { this.dispatch(id, method, payload); }
}

@Component
export struct AgentPanel {
  onTool: (method: string, payload: string) => Promise<string> = async () => { throw new Error('工作区未连接'); };
  private controller: webview.WebviewController = new webview.WebviewController();
  private bridge: AgentBridge = new AgentBridge();
  aboutToAppear(): void {
    this.bridge.dispatch = (id: number, method: string, payload: string) => { this.execute(id, method, payload); };
  }
  private async execute(id: number, method: string, payload: string): Promise<void> {
    let result = ''; let error = '';
    try {
      if (method === 'chat') {
        const input = JSON.parse(payload) as ChatRequest;
        if (!/^https:\/\/[^\s]+$/.test(input.base)) throw new Error('模型 API 地址须使用 HTTPS');
        const client = http.createHttp();
        try {
          const response = await client.request(input.base.replace(/\/+$/, '') + '/chat/completions', {
            method: http.RequestMethod.POST,
            header: { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + input.key },
            extraData: input.body, connectTimeout: 30000, readTimeout: 120000,
            expectDataType: http.HttpDataType.STRING
          });
          if (response.responseCode !== 200) throw new Error('模型请求失败 HTTP ' + response.responseCode);
          result = response.result as string;
        } finally { client.destroy(); }
      } else result = await this.onTool(method, payload);
    } catch (cause) { error = String(cause); }
    this.controller.runJavaScript('window.nativeReply(' + id + ',' + JSON.stringify(result) + ',' +
      JSON.stringify(error) + ')').catch(() => {});
  }
  build() {
    Web({src:$rawfile('agent/index.html'),controller:this.controller})
      .javaScriptAccess(true).fileAccess(false)
      .javaScriptProxy({object:this.bridge,name:'localAgent',methodList:['request'],controller:this.controller})
      .width('100%').height('100%').backgroundColor('#1e1e1e')
  }
}
