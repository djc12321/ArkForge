import { common } from '@kit.AbilityKit';
import { webSocket } from '@kit.NetworkKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { LocalDevice } from './LocalDevice';
import { AtomicFiles } from './AtomicFiles';
import { SourceMaps, SourcePosition } from './SourceMaps';

interface DebugLocation { scriptId: string; lineNumber: number; columnNumber: number; }
interface RemoteObject { type: string; description?: string; objectId?: string; value?: string | number | boolean; }
interface DebugScope { type: string; object: RemoteObject; }
export interface DebugFrame { callFrameId: string; functionName: string; url: string; location: DebugLocation; scopeChain: DebugScope[]; source?: SourcePosition; }
export interface DebugVariable { name: string; value: string; objectId?: string; }
interface RemoteProperty { name: string; value?: RemoteObject; }
interface PropertiesReply { result: RemoteProperty[]; }
interface DebugEvent { url?: string; scriptId?: string; callFrames?: DebugFrame[]; reason?: string; data?: RemoteObject; }
interface WireMessage { id?: number; method?: string; type?: string; instanceId?: number; result?: Object; params?: DebugEvent; error?: WireError; }
interface WireError { message: string; }
interface Pending { method: string; resolve: (text: string) => void; reject: (error: Error) => void; timer: number; }
interface Breakpoint { path: string; line: number; }
interface BreakpointReply { breakpointId?: string; }
interface InstalledBuild { root: string; project: string; bundleName: string; abilityName: string; }
export interface DebugState { connected: boolean; paused: boolean; status: string; frames: DebugFrame[]; variables: DebugVariable[]; }

/** Official Ark debugger protocol over this device's loopback HDC transport. */
export class DeviceDebugger {
  private control: webSocket.WebSocket | undefined = undefined;
  private engine: webSocket.WebSocket | undefined = undefined;
  private pending: Map<number, Pending> = new Map();
  private next: number = 0;
  private context: common.UIAbilityContext | undefined = undefined;
  private controlTarget: string = '';
  private engineTarget: string = '';
  private maps: SourceMaps | undefined = undefined;
  private project: string = '';
  private scripts: Map<string, string> = new Map();
  private installedBreakpoints: Map<string, string> = new Map();
  private instanceTimer: number = -1;
  private auditFile: string = '';
  private breakpoints: Breakpoint[] = [];
  private notify: (state: DebugState) => void;
  private state: DebugState = {connected:false,paused:false,status:'尚未连接调试器',frames:[],variables:[]};
  constructor(notify: (state: DebugState) => void) { this.notify = notify; }
  private audit(text: string): void {
    if (!this.auditFile) return;
    const file = fileIo.openSync(this.auditFile, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.APPEND);
    try { fileIo.writeSync(file.fd, new Date().toISOString() + ' ' + text + '\n'); } finally { fileIo.closeSync(file); }
  }
  private publish(): void { this.notify({connected:this.state.connected,paused:this.state.paused,status:this.state.status,frames:this.state.frames.slice(),variables:this.state.variables.slice()}); }
  private text(data: string | ArrayBuffer): string { return typeof data === 'string' ? data : new util.TextDecoder().decodeToString(new Uint8Array(data)); }

  private connectSocket(socket: webSocket.WebSocket, url: string): Promise<void> {
    // NetworkKit connect() resolves when the request is created, not when the
    // WebSocket handshake completes. Sending before 'open' loses the greeting.
    return new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => { reject(new Error('调试 WebSocket 连接超时')); }, 15000);
      socket.on('open', () => { clearTimeout(timer); this.audit('OPEN ' + url); resolve(); });
      socket.on('error', (error: BusinessError) => { clearTimeout(timer); reject(new Error('调试 WebSocket 连接失败：' + error.message)); });
      // Ark's local inspector transport does not implement the automatic
      // NetworkKit ping exchange. Keep this loopback connection direct.
      socket.connect(url, {proxy:'no-proxy',pingInterval:0,pongTimeout:0}).then((created: boolean) => {
        if (!created) { clearTimeout(timer); reject(new Error('未能创建调试 WebSocket 连接')); }
      }).catch((error: BusinessError) => { clearTimeout(timer); reject(new Error(error.message)); });
    });
  }

  private command(method: string, params: string = '{}'): Promise<string> {
    if (!this.engine) return Promise.reject(new Error('调试连接尚未建立'));
    const id = ++this.next;
    this.audit('SEND ' + id + ' ' + method + ' ' + params);
    return new Promise<string>((resolve, reject) => {
      const timer = setTimeout(() => { this.pending.delete(id); reject(new Error('调试命令超时：' + method)); }, 15000);
      this.pending.set(id, {method:method,resolve:resolve,reject:reject,timer:timer});
      this.engine?.send('{"id":' + id + ',"method":' + JSON.stringify(method) + ',"params":' + params + '}').catch((error: BusinessError) => {
        clearTimeout(timer); this.pending.delete(id); reject(new Error(error.message));
      });
    });
  }
  private receive(text: string): void {
    const message = JSON.parse(text) as WireMessage;
    if (message.id !== undefined) {
      this.audit('REPLY ' + message.id + (message.error ? ' ERROR ' + message.error.message : ' OK'));
      const request = this.pending.get(message.id);
      if (request) { clearTimeout(request.timer); this.pending.delete(message.id);
        if (message.error) request.reject(new Error(message.error.message)); else request.resolve(JSON.stringify(message.result || {})); }
      return;
    }
    const event = message.params;
    if (message.method === 'Debugger.paused') this.audit('PAUSED ' + (event?.reason || '') + ' ' + (event?.callFrames || []).map((frame: DebugFrame) => frame.url + ':' + frame.location.lineNumber).join(', '));
    if (message.method === 'Debugger.scriptParsed' && event?.scriptId && event.url) {
      this.scripts.set(event.scriptId, event.url);
      this.applyBreakpoints(event.url).catch((error: Error) => { this.state.status = error.message; this.publish(); });
    }
    if (message.method === 'Debugger.paused' && event) {
      this.pending.forEach((request: Pending, id: number) => {
        if (request.method === 'Debugger.pause') {
          clearTimeout(request.timer); this.pending.delete(id); request.resolve('{}');
        }
      });
      this.state.paused = true;
      this.state.status = event.reason === 'exception' ? '异常暂停：' + (event.data?.description || '') : '执行已暂停';
      this.state.frames = (event.callFrames || []).map((frame: DebugFrame) => {
        frame.url = frame.url || this.scripts.get(frame.location.scriptId) || '';
        frame.source = this.maps?.original(frame.url, frame.location.lineNumber, frame.location.columnNumber);
        return frame;
      });
      this.state.variables = []; this.publish();
      if (this.state.frames.length) this.inspectFrame(this.state.frames[0]).catch((error: Error) => { this.state.status = error.message; this.publish(); });
    }
    if (message.method === 'Debugger.resumed') { this.state.paused = false; this.state.frames = []; this.state.variables = []; this.state.status = '应用运行中'; this.publish(); }
  }
  async start(context: common.UIAbilityContext, project: string): Promise<void> {
    await this.stop(); this.context = context;
    if (!fileIo.accessSync(context.filesDir + '/debugger')) fileIo.mkdirSync(context.filesDir + '/debugger');
    this.auditFile = context.filesDir + '/debugger/session-' + Date.now() + '.log';
    AtomicFiles.write(context.filesDir + '/debugger/latest.txt', this.auditFile);
    const built = JSON.parse(fileIo.readTextSync(context.filesDir + '/offline-build/latest.json')) as InstalledBuild;
    if (built.project !== project) throw new Error('最近安装的应用属于其他工程，请先构建并安装当前工程');
    if (built.bundleName === 'com.djc.arkforge') throw new Error('请选择独立应用作为调试目标');
    this.project = built.project; this.maps = new SourceMaps(built.root);
    if (!fileIo.accessSync(built.root + '/source-maps.json')) throw new Error('最近安装的应用没有源码映射，请先重新构建');
    const saved = this.project + '/.arkforge/breakpoints.json';
    this.breakpoints = fileIo.accessSync(saved) ? JSON.parse(fileIo.readTextSync(saved)) as Breakpoint[] : [];
    this.state.status = '正在启动调试目标'; this.publish();
    try {
    const pid = await LocalDevice.prepareDebugger(context, built.bundleName, built.abilityName);
    this.controlTarget = 'ark:' + pid + '@' + built.bundleName;
    await LocalDevice.debugForward(context, 9230, this.controlTarget);
    this.control = webSocket.createWebSocket();
    await this.connectSocket(this.control, 'ws://127.0.0.1:9230');
    const instance = new Promise<number>((resolve, reject) => {
      const timer = setTimeout(() => { reject(new Error('等待方舟调试实例超时')); }, 15000);
      this.instanceTimer = timer;
      this.control?.on('message', (error: BusinessError, data: string | ArrayBuffer) => {
        if (error && error.code !== 0) { clearTimeout(timer); reject(new Error(error.message)); return; }
        const text = this.text(data);
        this.audit('CONTROL ' + text);
        const message = JSON.parse(text) as WireMessage;
        if (message.type === 'addInstance' && message.instanceId !== undefined) { clearTimeout(timer); resolve(message.instanceId); }
      });
    });
    await this.control.send('{"type":"connected"}');
    const id = await instance;
    this.engineTarget = 'ark:' + pid + (id ? '@' + id : '') + '@Debugger';
    await LocalDevice.debugForward(context, 9231, this.engineTarget);
    this.engine = webSocket.createWebSocket();
    this.engine.on('message', (error: BusinessError, data: string | ArrayBuffer) => {
      if (!error || error.code === 0) { try { this.receive(this.text(data)); } catch (failure) { this.state.status = String(failure); this.publish(); } }
    });
    const engine = this.engine;
    this.engine.on('close', (error: BusinessError, result: webSocket.CloseResult) => {
      this.audit('CLOSE ' + JSON.stringify(result));
      if (this.engine !== engine) return;
      this.state.connected = false; this.state.status = '调试连接已关闭；可读取应用日志查看退出原因'; this.publish();
    });
    await this.connectSocket(this.engine, 'ws://127.0.0.1:9231');
    await this.command('Runtime.enable'); await this.command('Debugger.enable');
    await this.command('Debugger.setPauseOnExceptions', '{"state":"uncaught"}');
    this.state.connected = true; this.state.status = '调试器已连接'; this.publish();
    await this.command('Runtime.runIfWaitingForDebugger');
    // Bring the editor back only after releasing the target's startup wait.
    // Starting another ability earlier can block on the same startup queue.
    await context.startAbility({bundleName:'com.djc.arkforge',abilityName:'EntryAbility'});
    } catch (error) { await this.stop(); throw new Error(String(error)); }
  }
  async inspectFrame(frame: DebugFrame): Promise<void> {
    const variables: DebugVariable[] = [];
    for (const scope of frame.scopeChain.filter((item: DebugScope) => item.type !== 'global')) {
      if (!scope.object.objectId) continue;
      const reply = JSON.parse(await this.command('Runtime.getProperties', '{"objectId":' + JSON.stringify(scope.object.objectId) + ',"ownProperties":true}')) as PropertiesReply;
      for (const property of reply.result || []) if (property.value) variables.push({name:property.name,value:String(property.value.value ?? property.value.description ?? property.value.type),objectId:property.value.objectId});
      if (variables.length >= 200) break;
    }
    this.state.variables = variables.slice(0, 200); this.publish();
    this.audit('VARIABLES ' + this.state.variables.length);
  }
  async action(method: string): Promise<void> {
    if (!['Debugger.resume','Debugger.pause','Debugger.stepOver','Debugger.stepInto','Debugger.stepOut'].includes(method)) throw new Error('无效调试操作');
    await this.command(method);
  }
  async toggle(project: string, path: string, line: number): Promise<void> {
    if (!path || line < 1) throw new Error('请选择可执行源码行');
    this.project = project;
    const saved = project + '/.arkforge/breakpoints.json';
    if (!this.breakpoints.length && fileIo.accessSync(saved)) this.breakpoints = JSON.parse(fileIo.readTextSync(saved)) as Breakpoint[];
    const present = this.breakpoints.find((item: Breakpoint) => item.path === path && item.line === line);
    if (present) this.breakpoints = this.breakpoints.filter((item: Breakpoint) => item !== present);
    else this.breakpoints.push({path:path,line:line});
    if (!fileIo.accessSync(project + '/.arkforge')) fileIo.mkdirSync(project + '/.arkforge', true);
    AtomicFiles.write(saved, JSON.stringify(this.breakpoints));
    this.state.status = (present ? '已移除' : '已保存') + '断点：' + path + ':' + line; this.publish();
    if (this.state.connected) for (const url of this.scripts.values()) await this.applyBreakpoints(url);
  }
  private async applyBreakpoints(url: string): Promise<void> {
    for (const key of this.installedBreakpoints.keys()) if (key.startsWith(url + '\n')) {
      await this.command('Debugger.removeBreakpoint', '{"breakpointId":' + JSON.stringify(this.installedBreakpoints.get(key)) + '}');
      this.installedBreakpoints.delete(key);
    }
    for (const point of this.breakpoints) {
      if (!SourceMaps.path(url).includes(point.path.replace(/\.(ets|ts)$/, ''))) continue;
      const generated = this.maps?.generated(point.path, point.line);
      if (generated === undefined) throw new Error('该行没有可执行源码映射：' + point.path + ':' + point.line);
      const result = JSON.parse(await this.command('Debugger.setBreakpointByUrl', '{"url":' + JSON.stringify(url) + ',"lineNumber":' + generated + ',"columnNumber":0}')) as BreakpointReply;
      if (result.breakpointId) this.installedBreakpoints.set(url + '\n' + point.path + ':' + point.line, result.breakpointId);
    }
  }
  async stop(): Promise<void> {
    if (this.instanceTimer >= 0) clearTimeout(this.instanceTimer);
    if (this.engine && this.state.paused) { try { await this.command('Debugger.resume'); } catch (_) {} }
    this.state.connected = false; this.state.paused = false; this.state.frames = []; this.state.variables = [];
    if (this.engine) { try { await this.engine.close(); } catch (_) {} this.engine = undefined; }
    if (this.control) { try { await this.control.close(); } catch (_) {} this.control = undefined; }
    if (this.context) {
      if (this.engineTarget) await LocalDevice.debugForward(this.context, 9231, this.engineTarget, true);
      if (this.controlTarget) await LocalDevice.debugForward(this.context, 9230, this.controlTarget, true);
    }
    this.engineTarget = ''; this.controlTarget = ''; this.scripts.clear(); this.installedBreakpoints.clear();
    for (const item of this.pending.values()) { clearTimeout(item.timer); item.reject(new Error('调试会话已结束')); }
    this.pending.clear(); this.state.status = '调试会话已结束'; this.publish();
  }
}
