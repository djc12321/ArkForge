import { asset } from '@kit.AssetStoreKit';
import { util } from '@kit.ArkTS';
import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { AtomicFiles } from './AtomicFiles';
import { http } from '@kit.NetworkKit';
interface ChatRequest { base: string; key?: string; body: string; }
interface ModelConfiguration { base: string; model: string; key?: string; }
export class AgentModel {
  private static active: http.HttpRequest | undefined = undefined;
  static cancel(): void { AgentModel.active?.destroy(); AgentModel.active = undefined; }
  private static settings(context: common.UIAbilityContext): ModelConfiguration {
    const path = context.filesDir + '/agent-model.json';
    return fileIo.accessSync(path) ? JSON.parse(fileIo.readTextSync(path)) as ModelConfiguration : {base:'https://api.deepseek.com',model:'deepseek-flash'};
  }
  private static async key(base: string): Promise<string> {
    const query: asset.AssetMap = new Map();
    query.set(asset.Tag.ALIAS,new util.TextEncoder().encodeInto('ArkForge.Agent:' + base));
    query.set(asset.Tag.RETURN_TYPE,asset.ReturnType.ALL);
    try {
      const records = await asset.query(query);
      return records.length ? new util.TextDecoder().decodeToString(records[0].get(asset.Tag.SECRET) as Uint8Array) : '';
    } catch (_) { return ''; }
  }
  static async configuration(context: common.UIAbilityContext): Promise<string> {
    const settings = AgentModel.settings(context);
    return JSON.stringify({base:settings.base,model:settings.model,hasKey:(await AgentModel.key(settings.base)).length > 0});
  }
  static async configure(context: common.UIAbilityContext,payload: string): Promise<string> {
    const settings = JSON.parse(payload) as ModelConfiguration;
    settings.base = settings.base.replace(/\/+$/, '');
    if (!/^https:\/\/[^\s]+$/.test(settings.base) || !settings.model.trim()) throw new Error('模型 API 地址或名称无效');
    if (settings.key) {
      const attributes: asset.AssetMap = new Map();
      attributes.set(asset.Tag.ALIAS,new util.TextEncoder().encodeInto('ArkForge.Agent:' + settings.base));
      attributes.set(asset.Tag.SECRET,new util.TextEncoder().encodeInto(settings.key));
      attributes.set(asset.Tag.CONFLICT_RESOLUTION,asset.ConflictResolution.OVERWRITE);
      await asset.add(attributes);
    }
    AtomicFiles.write(context.filesDir + '/agent-model.json',JSON.stringify({base:settings.base,model:settings.model}));
    return '模型配置已保存，密钥位于鸿蒙系统加密资产库';
  }
  static async request(context: common.UIAbilityContext,payload: string): Promise<string> {
    const input = JSON.parse(payload) as ChatRequest;
    if (!/^https:\/\/[^\s]+$/.test(input.base)) throw new Error('模型 API 地址须使用 HTTPS');
    if (AgentModel.active) throw new Error('已有模型请求正在执行');
    const key = input.key || await AgentModel.key(input.base);
    if (!key.trim()) throw new Error('请先输入模型 API Key');
    const client = http.createHttp();
    AgentModel.active = client;
    try {
      const response = await client.request(input.base.replace(/\/+$/, '') + '/chat/completions', {
        method:http.RequestMethod.POST,
        header:{'Content-Type':'application/json','Authorization':'Bearer ' + key},
        extraData:input.body,connectTimeout:30000,readTimeout:120000,expectDataType:http.HttpDataType.STRING
      });
      if (response.responseCode !== 200) {
        const hints: Record<string,string> = {'401':'API Key 无效或未授权','402':'账户余额不足','404':'请检查 API 地址和模型名称','429':'请求频率达到限制，请稍后重试','503':'模型服务暂时不可用'};
        throw new Error('模型请求失败 HTTP ' + response.responseCode + '：' + (hints[String(response.responseCode)] || '请检查模型服务状态'));
      }
      return response.result as string;
    } finally { if (AgentModel.active === client) AgentModel.active = undefined; client.destroy(); }
  }
}
