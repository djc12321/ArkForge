// Adapted from guantw/HapSign (MIT): login/browser_login.py, token/token_exchange.py,
// config.py and api/{cert,device,provision}_api.py. No desktop runtime or relay.
import { asset } from '@kit.AssetStoreKit';
import { util } from '@kit.ArkTS';
import { http } from '@kit.NetworkKit';
import { common } from '@kit.AbilityKit';
import { backgroundTaskManager } from '@kit.BackgroundTasksKit';
import { fileIo } from '@kit.CoreFileKit';
import resources from 'libarkforge_resources.so';
import signer from 'libsigntool.so';
import { SigningMaterials, SigningCredentials } from './SigningMaterials';
import { LocalDevice } from './LocalDevice';
import { AtomicFiles } from './AtomicFiles';

interface HuaweiAccount { jwt: string; access: string; uid: string; name: string; }
interface HuaweiUser { accessToken?: string; userName?: string; userId?: string; }
interface HuaweiJWT { userName?: string; userId?: string; }
interface HuaweiTeam { id: string; name?: string; teamName?: string; }
interface HuaweiPublicTeam { id: string; name: string; }
interface HuaweiCert { id: string; certName: string; certObjectId: string; }
interface HuaweiDevice { id: string; udid: string; }
interface HuaweiURL { newUrl: string; }
interface HuaweiReply {
  status?: boolean; userInfo?: HuaweiUser; ret?: HuaweiCode; code?: number;
  teams?: HuaweiTeam[]; certList?: HuaweiCert[]; list?: HuaweiDevice[];
  urlsInfo?: HuaweiURL[]; provisionFileUrl?: string; id?: string;
}
interface HuaweiCode { code: number; }
interface HuaweiCallback { pending?: boolean; tempToken?: string; siteId?: string; error?: string; }
interface HuaweiInput { id?: number; teamId?: string; udid?: string; certName?: string; }
interface HuaweiPending { certName: string; credentials: SigningCredentials; }
interface HuaweiStored { chunks?: number; version?: string; }
interface HuaweiFlow { id: number; expires: number; }

export class HuaweiSigning {
  private static readonly login: string = 'https://devecostudio.huawei.com';
  // The official CN page calls cn.devecostudio.huawei.com with Ide-Port.
  // Open it on that same origin: the global page's CORS preflight rejects Ide-Port.
  private static readonly browserLogin: string = 'https://cn.devecostudio.huawei.com';
  private static readonly cloud: string = 'https://connect-api.cloud.huawei.com';
  private static flow: HuaweiFlow | undefined;
  private static sequence: number = 0;
  private static exchanging: boolean = false;
  private static issuing: boolean = false;
  private static delayId: number = -1;
  private static lastDiagnostic: string = '';
  private static releaseDelay(): void {
    if (HuaweiSigning.delayId < 0) return;
    try {backgroundTaskManager.cancelSuspendDelay(HuaweiSigning.delayId);} catch (_) {}
    HuaweiSigning.delayId = -1;
  }
  private static query(suffix: string = ''): asset.AssetMap {
    const value: asset.AssetMap = new Map();
    value.set(asset.Tag.ALIAS,new util.TextEncoder().encodeInto('ArkForge.Huawei.Developer'+suffix));
    return value;
  }
  private static async readAsset(suffix: string = ''): Promise<Uint8Array | undefined> {
    const query = HuaweiSigning.query(suffix);query.set(asset.Tag.RETURN_TYPE,asset.ReturnType.ALL);
    try {
      const values = await asset.query(query);
      return values.length ? values[0].get(asset.Tag.SECRET) as Uint8Array : undefined;
    } catch (cause) {
      if ((cause as HuaweiCode).code === 24000002) return undefined;
      throw new Error('无法读取华为加密凭据，请解锁设备后重试');
    }
  }
  private static async removeParts(metadata: HuaweiStored): Promise<void> {
    if (!metadata.chunks || !metadata.version || metadata.chunks > 64) return;
    for (let i=0;i<metadata.chunks;i++) {
      try {await asset.remove(HuaweiSigning.query('.'+metadata.version+'.'+i));} catch (_) {}
    }
  }
  private static async saved(): Promise<HuaweiAccount | undefined> {
    const root = await HuaweiSigning.readAsset();if (!root) return undefined;
    const text = new util.TextDecoder().decodeToString(root);
    const metadata = JSON.parse(text) as HuaweiStored;
    if (!metadata.chunks) return JSON.parse(text) as HuaweiAccount;
    if (!Number.isInteger(metadata.chunks) || metadata.chunks < 1 || metadata.chunks > 64 || !metadata.version || !/^[a-f0-9]{32}$/.test(metadata.version)) throw new Error('华为凭据索引无效，请重新登录');
    const parts: Uint8Array[] = [];let length=0;
    for (let i=0;i<metadata.chunks;i++) {
      const part=await HuaweiSigning.readAsset('.'+metadata.version+'.'+i);
      if (!part) throw new Error('华为凭据不完整，请重新登录');
      parts.push(part);length+=part.length;
    }
    const bytes=new Uint8Array(length);let offset=0;
    for (const part of parts) {bytes.set(part,offset);offset+=part.length;}
    return JSON.parse(new util.TextDecoder().decodeToString(bytes)) as HuaweiAccount;
  }
  private static async writeAsset(suffix: string, bytes: Uint8Array): Promise<void> {
    const attributes=HuaweiSigning.query(suffix);attributes.set(asset.Tag.SECRET,bytes);
    attributes.set(asset.Tag.CONFLICT_RESOLUTION,asset.ConflictResolution.OVERWRITE);
    await asset.add(attributes);
  }
  private static async save(value: HuaweiAccount): Promise<void> {
    // AssetStore limits each SECRET to 1024 bytes. Publish the encrypted manifest
    // only after all versioned chunks exist, preserving the previous account on failure.
    const bytes=new util.TextEncoder().encodeInto(JSON.stringify(value));
    if (bytes.length>57600) throw new Error('华为凭据超过支持的存储大小');
    const old=await HuaweiSigning.readAsset();
    const metadata: HuaweiStored={chunks:Math.ceil(bytes.length/900),version:util.generateRandomUUID(true).replace(/-/g,'')};
    try {
      for (let i=0;i<Math.ceil(bytes.length/900);i++) await HuaweiSigning.writeAsset('.'+metadata.version+'.'+i,bytes.slice(i*900,(i+1)*900));
      await HuaweiSigning.writeAsset('',new util.TextEncoder().encodeInto(JSON.stringify(metadata)));
    } catch (cause) {await HuaweiSigning.removeParts(metadata);throw new Error('安全保存华为凭据失败，请重试');}
    if (old) {
      const previous=JSON.parse(new util.TextDecoder().decodeToString(old)) as HuaweiStored;
      if (previous.version!==metadata.version) await HuaweiSigning.removeParts(previous);
    }
  }
  private static form(values: string[][]): string {
    return values.map((pair: string[]) => encodeURIComponent(pair[0])+'='+encodeURIComponent(pair[1])).join('&');
  }
  private static async request(url: string, body: string = '', headers: Record<string,string> = {}, method?: http.RequestMethod): Promise<string> {
    if (!url.startsWith(HuaweiSigning.login+'/authrouter/') && !url.startsWith(HuaweiSigning.cloud+'/api/')) throw new Error('签名服务地址无效');
    const client = http.createHttp();
    let status: number = 0;
    try {
      const response = await client.request(url,{method:method || (body ? http.RequestMethod.POST : http.RequestMethod.GET),
        header:headers,extraData:body,connectTimeout:20000,readTimeout:30000,expectDataType:http.HttpDataType.STRING});
      status = response.responseCode;
      if (response.responseCode !== 200) throw new Error('华为开发者服务请求失败 HTTP '+response.responseCode);
      return response.result as string;
    } catch (cause) {
      // Do not return network exceptions containing a temp-token URL or credentials.
      if (status > 0 && status !== 200) throw new Error('华为开发者服务请求失败 HTTP '+status);
      throw new Error('无法连接华为开发者服务，请检查网络后重试');
    } finally { client.destroy(); }
  }
  private static async api(account: HuaweiAccount, path: string, body: string = '', team: string = '', json: boolean = false, method?: http.RequestMethod): Promise<HuaweiReply> {
    const headers: Record<string,string> = {'oauth2Token':account.access,'uid':account.uid,'User-Agent':'Chrome/49.0.2623.75','Accept-Language':'zh-CN'};
    if (team) headers['teamId']=team;
    if (body) headers['Content-Type']=json ? 'application/json' : 'application/x-www-form-urlencoded';
    const reply = JSON.parse(await HuaweiSigning.request(HuaweiSigning.cloud+path,body,headers,method)) as HuaweiReply;
    const code = reply.ret ? reply.ret.code : reply.code;
    if (code !== undefined && Number(code) !== 0) throw new Error('华为签名服务返回错误 '+code+'，请检查开发者实名认证、证书/设备配额或重新登录');
    return reply;
  }
  private static async refresh(): Promise<HuaweiAccount> {
    const account = await HuaweiSigning.saved();if (!account) throw new Error('请先登录华为开发者账号');
    const reply = JSON.parse(await HuaweiSigning.request(HuaweiSigning.login+'/authrouter/auth/api/jwToken/check','',{'jwtToken':account.jwt,'refresh':'true'})) as HuaweiReply;
    if (!reply.status || !reply.userInfo?.accessToken) throw new Error('华为开发者登录已失效，请清除登录凭据后重新授权');
    account.access=reply.userInfo.accessToken;await HuaweiSigning.save(account);return account;
  }
  static async execute(context: common.UIAbilityContext, action: string, payload: string): Promise<string> {
    const input = JSON.parse(payload) as HuaweiInput;
    if (action === 'status') { const account=await HuaweiSigning.saved();return JSON.stringify({signedIn:!!account,name:account?.name||''}); }
    if (action === 'cancel') {
      if (!input.id || HuaweiSigning.flow?.id === input.id) { HuaweiSigning.flow=undefined;resources.cancelHuaweiCallback();HuaweiSigning.releaseDelay(); }
      return '{}';
    }
    if (action === 'logout') {
      if (HuaweiSigning.issuing) throw new Error('签名申请正在执行，请等待完成后退出');
      HuaweiSigning.flow=undefined;resources.cancelHuaweiCallback();HuaweiSigning.releaseDelay();
      const previous=await HuaweiSigning.readAsset();
      try { await asset.remove(HuaweiSigning.query()); } catch (cause) { if ((cause as HuaweiCode).code !== 24000002) throw new Error('清除华为凭据失败'); }
      if (previous) await HuaweiSigning.removeParts(JSON.parse(new util.TextDecoder().decodeToString(previous)) as HuaweiStored);
      return '{}';
    }
    if (action === 'start') {
      const account=await HuaweiSigning.saved();
      if (account) return JSON.stringify({authenticated:true,name:account.name});
      if (HuaweiSigning.exchanging) throw new Error('华为授权正在处理，请稍后');
      const code = util.generateRandomUUID(true).replace(/-/g,'');
      const port = resources.startHuaweiCallback(code);
      const flow: HuaweiFlow = {id:++HuaweiSigning.sequence,expires:Date.now()+600000};HuaweiSigning.flow=flow;
      HuaweiSigning.releaseDelay();
      try {
        const delay = backgroundTaskManager.requestSuspendDelay('等待华为网页登录回调',() => {
          if (HuaweiSigning.flow===flow) {HuaweiSigning.flow=undefined;resources.cancelHuaweiCallback();}
          HuaweiSigning.delayId=-1;
        });
        HuaweiSigning.delayId=delay.requestId;
      } catch (_) {}
      try { await context.openLink(HuaweiSigning.browserLogin+'/console/DevEcoIDE/apply?port='+port+'&appid=1007&code='+code); }
      catch (_) { HuaweiSigning.flow=undefined;resources.cancelHuaweiCallback();HuaweiSigning.releaseDelay();throw new Error('无法打开系统浏览器，请重试华为登录'); }
      return JSON.stringify({id:flow.id,expiresIn:600});
    }
    if (action === 'poll') {
      const diagnostic = resources.huaweiCallbackDiagnostics();
      if (diagnostic !== HuaweiSigning.lastDiagnostic) {
        HuaweiSigning.lastDiagnostic = diagnostic;
        AtomicFiles.write(context.filesDir+'/huawei-login-diagnostics.json',diagnostic);
      }
      const flow=HuaweiSigning.flow;
      if (!flow || flow.id!==input.id) return JSON.stringify({state:'cancelled'});
      if (Date.now()>flow.expires) {HuaweiSigning.flow=undefined;resources.cancelHuaweiCallback();HuaweiSigning.releaseDelay();throw new Error('华为登录超时，请重试');}
      if (HuaweiSigning.exchanging) return JSON.stringify({state:'pending'});
      const callback = JSON.parse(resources.takeHuaweiCallback()) as HuaweiCallback;
      if (callback.pending) return JSON.stringify({state:'pending'});
      if (callback.error || !callback.tempToken) { HuaweiSigning.flow=undefined;HuaweiSigning.releaseDelay();throw new Error('华为登录已取消或未返回临时令牌'); }
      HuaweiSigning.exchanging=true;
      try {
        const sites: Record<string,string> = {'1':'CN','5':'SG','7':'DE','8':'RU'};
        const jwt = (await HuaweiSigning.request(HuaweiSigning.login+'/authrouter/auth/api/temptoken/check?'+HuaweiSigning.form([['tempToken',callback.tempToken],['site',sites[callback.siteId||'1']||'CN'],['version','5.0.5'],['appid','1007']]))).trim().replace(/^"|"$/g,'');
        const reply = JSON.parse(await HuaweiSigning.request(HuaweiSigning.login+'/authrouter/auth/api/jwToken/check','',{'jwtToken':jwt,'refresh':'false'})) as HuaweiReply;
        if (!reply.status || !reply.userInfo?.accessToken) throw new Error('华为登录令牌交换未通过');
        let segment=jwt.split('.')[1];if (!segment) throw new Error('华为登录返回的身份信息无效');
        segment=segment.replace(/-/g,'+').replace(/_/g,'/');while(segment.length%4)segment+='=';
        const identity=JSON.parse(new util.TextDecoder().decodeToString(new util.Base64Helper().decodeSync(segment))) as HuaweiJWT;
        if (!identity.userId) throw new Error('华为登录缺少用户标识');
        if (HuaweiSigning.flow!==flow) return JSON.stringify({state:'cancelled'});
        await HuaweiSigning.save({jwt:jwt,access:reply.userInfo.accessToken,uid:String(identity.userId),name:identity.userName||'华为开发者'});
        HuaweiSigning.flow=undefined;resources.cancelHuaweiCallback();HuaweiSigning.releaseDelay();
        await context.startAbility({bundleName:context.applicationInfo.name,abilityName:'EntryAbility'}).catch(()=>{});
        return JSON.stringify({state:'authenticated',name:identity.userName||'华为开发者'});
      } finally {HuaweiSigning.exchanging=false;}
    }
    if (action === 'revokeCertificate') {
      if (HuaweiSigning.issuing) throw new Error('签名申请正在执行，请等待完成');
      const account=await HuaweiSigning.refresh();
      const teams=(await HuaweiSigning.api(account,'/api/ups/user-permission-service/v1/user-team-list')).teams||[];
      if (!teams.some((team: HuaweiTeam)=>String(team.id)===input.teamId)) throw new Error('所选开发者团队无效');
      if (!input.certName) throw new Error('请明确选择要撤销的证书');
      const certificates=(await HuaweiSigning.api(account,'/api/cps/harmony-cert-manage/v1/cert/list','',input.teamId)).certList||[];
      const matches=certificates.filter((item: HuaweiCert)=>item.certName===input.certName);
      if (matches.length!==1 || !matches[0].id) throw new Error('证书记录已变化，请重新核对后再操作');
      await HuaweiSigning.api(account,'/api/cps/harmony-cert-manage/v1/cert/delete',JSON.stringify({certIds:[String(matches[0].id)]}),input.teamId,true,http.RequestMethod.DELETE);
      return JSON.stringify({revoked:input.certName});
    }
    if (action === 'certificates') {
      const account=await HuaweiSigning.refresh();
      const teams=(await HuaweiSigning.api(account,'/api/ups/user-permission-service/v1/user-team-list')).teams||[];
      if (!teams.some((team: HuaweiTeam)=>String(team.id)===input.teamId)) throw new Error('所选开发者团队无效');
      const certificates=(await HuaweiSigning.api(account,'/api/cps/harmony-cert-manage/v1/cert/list','',input.teamId)).certList||[];
      return JSON.stringify({names:certificates.map((item: HuaweiCert): string=>item.certName)});
    }
    if (action === 'prepare') {
      const account=await HuaweiSigning.refresh();
      const reply=await HuaweiSigning.api(account,'/api/ups/user-permission-service/v1/user-team-list');
      if (!reply.teams?.length) throw new Error('未取得华为开发者团队，请先在官网完成开发者注册及实名认证');
      let udid='';try {udid=await LocalDevice.signingUdid(context);} catch (_) {}
      return JSON.stringify({name:account.name,teams:reply.teams.map((team: HuaweiTeam): HuaweiPublicTeam => { const value: HuaweiPublicTeam = {id:String(team.id),name:team.teamName||team.name||String(team.id)}; return value; }),udid:udid});
    }
    throw new Error('未知的华为签名操作');
  }
  private static async download(account: HuaweiAccount, source: string, team: string, path: string): Promise<void> {
    const reply=await HuaweiSigning.api(account,'/api/amis/app-manage/v1/objects/url/reapply',HuaweiSigning.form([['sourceUrls',source]]),team);
    const url=reply.urlsInfo?.[0]?.newUrl||'';
    const match=/^https:\/\/([^/:?#]+)(?:\/|$)/.exec(url);
    // Match HapSign: download the signed URL returned by Huawei's authenticated
    // reapply API. Storage/CDN domains vary by region; never forward account headers.
    // Signed Profile/certificate validation remains mandatory before activation.
    if (!match || !/^[A-Za-z0-9][A-Za-z0-9.-]*\.[A-Za-z]{2,}$/.test(match[1]) || /(?:^|\.)(localhost|local|internal)$/i.test(match[1])) throw new Error('华为返回的签名下载地址无效');
    const client=http.createHttp();
    try {
      const response=await client.request(url,{method:http.RequestMethod.GET,connectTimeout:20000,readTimeout:30000,expectDataType:http.HttpDataType.ARRAY_BUFFER});
      const data=response.result as ArrayBuffer;
      if(response.responseCode!==200||data.byteLength<1||data.byteLength>16*1024*1024)throw new Error('下载签名材料失败');
      const file=fileIo.openSync(path,fileIo.OpenMode.CREATE|fileIo.OpenMode.WRITE_ONLY|fileIo.OpenMode.TRUNC);
      try {fileIo.writeSync(file.fd,data);} finally {fileIo.closeSync(file);}
    } catch (_) {throw new Error('下载华为签名材料失败，请重试');} finally {client.destroy();}
  }
  static async issue(context: common.UIAbilityContext,bundle: string,teamId: string,udid: string,report: (text: string)=>void): Promise<string> {
    if (HuaweiSigning.issuing) throw new Error('已有签名申请正在执行');
    if (!/^[A-Za-z0-9_.]+$/.test(bundle)||!teamId||!/^[A-Fa-f0-9]{64}$/.test(udid))throw new Error('包名、开发者团队或设备 UDID 无效');
    HuaweiSigning.issuing=true;
    let directory='',pendingAlias='',pendingSaved=false,completed=false;
    try {
      const account=await HuaweiSigning.refresh();
      const teams=(await HuaweiSigning.api(account,'/api/ups/user-permission-service/v1/user-team-list')).teams||[];
      if(!teams.some((team: HuaweiTeam)=>String(team.id)===teamId))throw new Error('所选开发者团队无效');
      pendingAlias='.pending.'+account.uid+'.'+teamId+'.'+bundle;
      directory=context.filesDir+'/native-toolchain/huawei-signing-pending/'+encodeURIComponent(account.uid+'-'+teamId+'-'+bundle);
      if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory,true);
      const previous=await HuaweiSigning.readAsset(pendingAlias);
      let pending: HuaweiPending;
      if (previous && fileIo.accessSync(directory+'/key.p12') && fileIo.accessSync(directory+'/key.csr')) {
        pending=JSON.parse(new util.TextDecoder().decodeToString(previous)) as HuaweiPending;
        pendingSaved=true;
        report('> ArkForge :resumeHuaweiSigning');
      } else {
        const password=util.generateRandomUUID(true);
        const credentials: SigningCredentials={keyAlias:'arkforgeDebug',signAlg:'SHA256withECDSA',storePassword:password,keyPassword:password};
        const commonArgs: string[]=['-keyAlias',credentials.keyAlias,'-keyPwd',credentials.keyPassword,'-keystoreFile',directory+'/key.p12','-keystorePwd',credentials.storePassword];
        report('> ArkForge :generateSigningKey');
        if(await signer.runSigner(['generate-keypair',...commonArgs,'-keyAlg','ECC','-keySize','NIST-P-256'])!==0)throw new Error('本机生成签名密钥失败');
        if(await signer.runSigner(['generate-csr',...commonArgs,'-signAlg',credentials.signAlg,'-subject','CN=ArkForge Debug','-outFile',directory+'/key.csr'])!==0)throw new Error('本机生成证书申请失败');
        pending={certName:'ArkForge_'+bundle.slice(0,48)+'_'+Date.now()+'.cer',credentials:credentials};
        await HuaweiSigning.writeAsset(pendingAlias,new util.TextEncoder().encodeInto(JSON.stringify(pending)));
        pendingSaved=true;
      }
      const credentials=pending.credentials,certName=pending.certName;
      report('> ArkForge :requestHuaweiCertificate');
      // Recover from an interrupted request before creating anything new.
      let certs=(await HuaweiSigning.api(account,'/api/cps/harmony-cert-manage/v1/cert/list','',teamId)).certList||[];
      let certificate=certs.find((item: HuaweiCert)=>item.certName===certName);
      if (!certificate) {
        await HuaweiSigning.api(account,'/api/cps/harmony-cert-manage/v1/cert/add',HuaweiSigning.form([['csr',fileIo.readTextSync(directory+'/key.csr')],['certName',certName],['certType','1']]),teamId);
        certs=(await HuaweiSigning.api(account,'/api/cps/harmony-cert-manage/v1/cert/list','',teamId)).certList||[];
        certificate=certs.find((item: HuaweiCert)=>item.certName===certName);
      }
      if(!certificate?.id||!certificate.certObjectId)throw new Error('华为未返回新申请的调试证书');
      await HuaweiSigning.download(account,certificate.certObjectId,teamId,directory+'/app.cer');
      report('> ArkForge :registerSigningDevice');
      // Reuse a registered device; do not delete or replace any account resources.
      let devices=(await HuaweiSigning.api(account,'/api/cps/device-manage/v1/device/list?encodeFlag=0&start=1&pageSize=100','',teamId)).list||[];
      let device=devices.find((item: HuaweiDevice)=>item.udid.toLowerCase()===udid.toLowerCase());
      if(!device){
        await HuaweiSigning.api(account,'/api/cps/device-manage/v1/device/add',HuaweiSigning.form([['deviceName','ArkForge_'+Date.now()],['udid',udid],['deviceType','4']]),teamId);
        devices=(await HuaweiSigning.api(account,'/api/cps/device-manage/v1/device/list?encodeFlag=0&start=1&pageSize=100','',teamId)).list||[];
        device=devices.find((item: HuaweiDevice)=>item.udid.toLowerCase()===udid.toLowerCase());
      }
      if(!device?.id)throw new Error('华为未返回当前调试设备记录');
      report('> ArkForge :requestHuaweiProfile');
      const body=JSON.stringify({certList:[String(certificate.id)],packageName:bundle,deviceList:[String(device.id)],provisionName:'ArkForge_'+bundle.slice(0,80),aclPermissionList:[]});
      const profile=await HuaweiSigning.api(account,'/api/cps/provision-manage/v1/ide/test/provision/add',body,teamId,true);
      if(!profile.provisionFileUrl)throw new Error('华为未返回调试 Profile');
      await HuaweiSigning.download(account,profile.provisionFileUrl,teamId,directory+'/profile.p7b');
      report('> ArkForge :validateAndActivateSigning');
      await SigningMaterials.importFiles(context,[directory+'/key.p12',directory+'/app.cer',directory+'/profile.p7b'],credentials,bundle);
      completed=true;
      return JSON.stringify({cancelled:false,bundleName:bundle});
    } finally {
      // Keep failed attempts in the app sandbox and their password in AssetStore.
      // Retry resumes the same key/certificate instead of consuming another quota.
      if (completed || !pendingSaved) {
        for(const name of ['key.p12','key.csr','app.cer','profile.p7b']) {
          try {if(directory && fileIo.accessSync(directory+'/'+name))fileIo.unlinkSync(directory+'/'+name);} catch (_) {}
        }
        if (pendingAlias) {try {await asset.remove(HuaweiSigning.query(pendingAlias));} catch (_) {}}
      }
      HuaweiSigning.issuing=false;
    }
  }
}
