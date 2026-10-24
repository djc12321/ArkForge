import { asset } from '@kit.AssetStoreKit';
import { util } from '@kit.ArkTS';
import { http } from '@kit.NetworkKit';
import { common } from '@kit.AbilityKit';

interface GitHubReply {
  device_code?: string; user_code?: string; verification_uri?: string;
  expires_in?: number; refresh_token_expires_in?: number; interval?: number; error?: string;
  access_token?: string; refresh_token?: string; scope?: string;
  login?: string; id?: number; avatar_url?: string;
}
interface AccountSecret { token: string; login: string; id: number; scope: string; avatar?: string; refresh?: string; expiresAt?: number; refreshExpiresAt?: number; }
interface LoginFlow { id: number; device: string; expires: number; interval: number; next: number; }
interface AccountInput { id?: number; token?: string; }

/** GitHub device flow. Tokens never cross the workbench bridge or enter logs/files. */
export class GitHubAccount {
  private static readonly clientId: string = 'Ov23limi323d3d8QrHSK';
  private static flow: LoginFlow | undefined = undefined;
  private static sequence: number = 0;
  private static busy: boolean = false;
  private static query(): asset.AssetMap {
    const query: asset.AssetMap = new Map();
    query.set(asset.Tag.ALIAS, new util.TextEncoder().encodeInto('ArkForge.GitHub.Account'));
    return query;
  }
  private static async saved(): Promise<AccountSecret | undefined> {
    const query = GitHubAccount.query();
    query.set(asset.Tag.RETURN_TYPE, asset.ReturnType.ALL);
    try {
      const records = await asset.query(query);
      if (!records.length) return undefined;
      return JSON.parse(new util.TextDecoder().decodeToString(records[0].get(asset.Tag.SECRET) as Uint8Array)) as AccountSecret;
    } catch (cause) {
      // AssetStore returns NOT_FOUND (24000002) for an unconfigured account.
      const code = (cause as ErrorWithCode).code;
      if (code === 24000002) return undefined;
      throw new Error('无法读取 GitHub 加密账户，请解锁设备后重试');
    }
  }
  private static async request(url: string, body: string = '', token: string = ''): Promise<GitHubReply> {
    const client = http.createHttp();
    const header: Record<string, string> = { 'Accept': 'application/json', 'User-Agent': 'ArkForge' };
    if (body) header['Content-Type'] = 'application/x-www-form-urlencoded';
    if (token) header['Authorization'] = 'Bearer ' + token;
    try {
      const response = await client.request(url, {
        method: body ? http.RequestMethod.POST : http.RequestMethod.GET,
        header: header, extraData: body, connectTimeout: 20000, readTimeout: 30000,
        expectDataType: http.HttpDataType.STRING
      });
      if (response.responseCode !== 200) throw new Error('GitHub 请求失败 HTTP ' + response.responseCode);
      return JSON.parse(response.result as string) as GitHubReply;
    } finally { client.destroy(); }
  }
  private static async persist(secret: AccountSecret): Promise<void> {
    const attributes = GitHubAccount.query();
    attributes.set(asset.Tag.SECRET,new util.TextEncoder().encodeInto(JSON.stringify(secret)));
    attributes.set(asset.Tag.CONFLICT_RESOLUTION,asset.ConflictResolution.OVERWRITE);
    await asset.add(attributes);
  }
  private static async store(token: string, scope: string, flow?: LoginFlow, reply?: GitHubReply): Promise<string> {
    const user = await GitHubAccount.request('https://api.github.com/user', '', token);
    if (!user.login || !user.id) throw new Error('GitHub 身份验证失败，未保存令牌');
    if (flow && GitHubAccount.flow !== flow) return JSON.stringify({state:'cancelled'});
    const secret: AccountSecret = {token:token,login:user.login,id:user.id,scope:scope,avatar:user.avatar_url};
    if (reply?.expires_in) {
      if (!reply.refresh_token || reply.expires_in < 1) throw new Error('GitHub 短期令牌缺少有效的刷新材料');
      secret.refresh = reply.refresh_token;
      secret.expiresAt = Date.now()+reply.expires_in*1000;
      secret.refreshExpiresAt = reply.refresh_token_expires_in ? Date.now()+reply.refresh_token_expires_in*1000 : undefined;
    }
    await GitHubAccount.persist(secret);
    return JSON.stringify({state:'authenticated',login:user.login,id:user.id,scope:scope});
  }
  static async execute(method: string, payload: string): Promise<string> {
    const input = JSON.parse(payload) as AccountInput;
    if (method === 'status') {
      const saved = await GitHubAccount.saved();
      return JSON.stringify({signedIn:saved !== undefined,login:saved?.login || '',scope:saved?.scope || '',
        avatar:saved ? (saved.avatar || 'https://avatars.githubusercontent.com/u/' + saved.id + '?s=64&v=4') : '',transportReady:true});
    }
    if (method === 'cancel') {
      if (GitHubAccount.flow?.id === input.id) GitHubAccount.flow = undefined;
      return '{}';
    }
    if (GitHubAccount.busy) throw new Error('GitHub 账户操作正在进行，请稍候');
    GitHubAccount.busy = true;
    try {
      if (method === 'logout') {
        GitHubAccount.flow = undefined;
        if (await GitHubAccount.saved()) await asset.remove(GitHubAccount.query());
        return '{}';
      }
      if (method === 'verify' || method === 'refresh') {
        const secret = await GitHubAccount.saved();
        if (!secret) throw new Error('请先登录 GitHub');
        if (secret.expiresAt && (method === 'refresh' || Date.now() >= secret.expiresAt-60000)) {
          if (!secret.refresh || (secret.refreshExpiresAt && Date.now() >= secret.refreshExpiresAt)) throw new Error('GitHub 刷新令牌已失效，请重新登录');
          const reply = await GitHubAccount.request('https://github.com/login/oauth/access_token',
            'client_id=' + GitHubAccount.clientId + '&grant_type=refresh_token&refresh_token=' + encodeURIComponent(secret.refresh));
          if (reply.error || !reply.access_token || !reply.refresh_token || !reply.expires_in) throw new Error('GitHub 令牌刷新失败：' + (reply.error || '无效响应') + '；请重新登录');
          secret.token = reply.access_token; secret.refresh = reply.refresh_token;
          secret.expiresAt = Date.now()+reply.expires_in*1000;
          secret.refreshExpiresAt = reply.refresh_token_expires_in ? Date.now()+reply.refresh_token_expires_in*1000 : undefined;
          // Persist the rotated pair before further network requests: the old pair is now invalid.
          await GitHubAccount.persist(secret);
        }
        const user = await GitHubAccount.request('https://api.github.com/user','',secret.token);
        if (!user.id || user.id !== secret.id || !user.login) throw new Error('GitHub 账户核对失败，请重新登录');
        secret.login = user.login;
        secret.avatar = user.avatar_url;
        await GitHubAccount.persist(secret);
        return JSON.stringify({state:'authenticated',login:user.login,id:user.id,expiresAt:secret.expiresAt || 0});
      }
      if (method === 'token') {
        if (!input.token || /\s/.test(input.token) || input.token.length > 1024) throw new Error('请输入有效的 GitHub 访问令牌');
        return await GitHubAccount.store(input.token, 'token');
      }
      if (method === 'start') {
        GitHubAccount.flow = undefined;
        const reply = await GitHubAccount.request('https://github.com/login/device/code',
          'client_id=' + GitHubAccount.clientId + '&scope=repo%20read%3Auser');
        if (reply.error) throw new Error('GitHub 设备授权失败：' + reply.error + '；请检查 OAuth App 的 Device Flow 开关');
        if (!reply.device_code || !reply.user_code || reply.verification_uri !== 'https://github.com/login/device' ||
          !reply.expires_in || reply.expires_in < 1 || !reply.interval || reply.interval < 1) throw new Error('GitHub 返回无效的设备授权响应');
        const flow: LoginFlow = {id:++GitHubAccount.sequence,device:reply.device_code,
          expires:Date.now()+reply.expires_in*1000,interval:reply.interval,next:Date.now()+reply.interval*1000};
        GitHubAccount.flow = flow;
        return JSON.stringify({id:flow.id,code:reply.user_code,interval:flow.interval,expiresIn:reply.expires_in});
      }
      if (method === 'poll') {
        const flow = GitHubAccount.flow;
        if (!flow || flow.id !== input.id) return JSON.stringify({state:'cancelled'});
        if (Date.now() >= flow.expires) { GitHubAccount.flow = undefined; return JSON.stringify({state:'expired_token'}); }
        if (Date.now() < flow.next) return JSON.stringify({state:'authorization_pending',interval:Math.ceil((flow.next-Date.now())/1000)});
        flow.next = Date.now()+flow.interval*1000;
        const reply = await GitHubAccount.request('https://github.com/login/oauth/access_token',
          'client_id=' + GitHubAccount.clientId + '&device_code=' + encodeURIComponent(flow.device) +
          '&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code');
        if (GitHubAccount.flow !== flow) return JSON.stringify({state:'cancelled'});
        if (reply.error === 'authorization_pending') return JSON.stringify({state:reply.error,interval:flow.interval});
        if (reply.error === 'slow_down') {
          flow.interval += 5; flow.next = Date.now()+flow.interval*1000;
          return JSON.stringify({state:'authorization_pending',interval:flow.interval});
        }
        if (reply.error) { GitHubAccount.flow = undefined; throw new Error('GitHub 授权未完成：' + reply.error); }
        if (!reply.access_token) throw new Error('GitHub 未返回访问令牌');
        const result = await GitHubAccount.store(reply.access_token, reply.scope || '', flow, reply);
        GitHubAccount.flow = undefined;
        return result;
      }
      throw new Error('不支持的 GitHub 账户操作');
    } finally { GitHubAccount.busy = false; }
  }
  // Native-only caller. Never expose this method through execute()/the workbench bridge.
  static async gitCredential(): Promise<string> {
    let secret = await GitHubAccount.saved();
    if (!secret) return '';
    if (secret.expiresAt && Date.now() >= secret.expiresAt-60000) {
      await GitHubAccount.execute('refresh','{}');
      secret = await GitHubAccount.saved();
    }
    return secret?.token || '';
  }
  static async open(context: common.UIAbilityContext, page: string): Promise<string> {
    const links: Record<string,string> = {};
    links['github'] = 'https://github.com/login/device';
    links['tokens'] = 'https://github.com/settings/personal-access-tokens/new';
    links['githubAccess'] = 'https://github.com/settings/connections/applications/' + GitHubAccount.clientId;
    links['huawei'] = 'https://developer.huawei.com/consumer/cn/';
    links['certificates'] = 'https://developer.huawei.com/consumer/cn/service/josp/agc/index.html';
    links['signingHelp'] = 'https://developer.huawei.com/consumer/en/doc/app/agc-help-add-debugcert-0000001914263178';
    links['signingAutoHelp'] = 'https://developer.huawei.com/consumer/cn/doc/doccenter-deveco-studio/ide-signing-auto';
    const url = links[page];
    if (!url) throw new Error('未知的官方页面');
    await context.openLink(url);
    return '{}';
  }
}
interface ErrorWithCode { code: number; }
