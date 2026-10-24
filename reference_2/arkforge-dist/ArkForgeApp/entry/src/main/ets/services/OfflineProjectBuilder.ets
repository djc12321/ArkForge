import { ResourceCompiler, BuiltResources } from './ResourceCompiler';
import { AtomicFiles } from './AtomicFiles';
import { SourceMapEntry, SourceMapFile } from './SourceMaps';
import { SDKWebRuntime } from './SDKWebRuntime';
import { ArkUIWebRuntime } from './ArkUIWebRuntime';
import { common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { worker, MessageEvents, util } from '@kit.ArkTS';
import { BuildProgress } from './BuildConsole';
import { NativeToolchain } from './NativeToolchain';
import { LocalDevice } from './LocalDevice';
import { SDKProjectChecker, SDKDiagnostic, SDKResult } from './SDKProjectChecker';

interface ResourceFile { path: string; sha256: string; }
interface ResourceCache {
  files: ResourceFile[];
  module?: string;
  bundleName?: string;
  abilityName?: string;
  cacheSha256?: string;
  resources?: Record<string, Record<string, Record<string, number>>>;
}
interface WorkerReply { ok: boolean; output?: string; outputs?: TransformedFile[]; error?: string; errors?: CompileDiagnostic[]; status?: number; diagnostics?: SDKDiagnostic[]; }
interface TransformedFile { fileName: string; output: string; sourceMap?: string; generatedLineMap?: number[]; }
interface CompileDiagnostic { message: string; start: number; line: number; column: number; }
interface WorkerRequest { kind: string; bundlePath: string; source: string; fileName: string; args: string[]; }
interface SourceEvidence { path: string; sha256: string; }
interface PreparedBuild { id: string; project: string; signedHap: string; signedHapSha256: string; bundleName: string; }
interface CachedBuild {
  project: string; root: string; sources: SourceEvidence[]; abcSha256: string;
  compilerSha256?: string; resourceManifestSha256?: string;
  signingFingerprint?: string; signedHap?: string; signedHapSha256?: string; hapSha256?: string;
  sdkInputKey?: string; sdkChecked?: boolean;
  resourceArchive?: string; resourceArchiveSha256?: string; resourceSymbols?: Record<string, Record<string, Record<string, number>>>;
}

/** Offline incremental build: source is recompiled; unchanged resources use a checked SDK cache. */
export class OfflineProjectBuilder {
  private static running: boolean = false;
  private static write(path: string, text: string): void {
    AtomicFiles.write(path, text);
  }
  private static async raw(context: common.UIAbilityContext, name: string, path: string): Promise<void> {
    const bytes = await context.resourceManager.getRawFileContent(name);
    const file = fileIo.openSync(path, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, bytes.buffer); } finally { fileIo.closeSync(file); }
  }
  private static collect(root: string, relative: string, files: string[]): void {
    for (const name of fileIo.listFileSync(root + '/' + relative)) {
      const next = relative ? relative + '/' + name : name;
      if (fileIo.statSync(root + '/' + next).isDirectory()) OfflineProjectBuilder.collect(root, next, files);
      else if ((name.endsWith('.ets') || name.endsWith('.ts')) && !/\.d\.(ts|ets)$/.test(name)) files.push(next);
    }
  }
  private static request(thread: worker.ThreadWorker, request: WorkerRequest): Promise<WorkerReply> {
    return new Promise<WorkerReply>((resolve, reject) => {
      thread.onmessage = (event: MessageEvents): void => {
        try { resolve(JSON.parse(event.data as string) as WorkerReply); }
        catch (_) { reject(new Error('编译器返回无效结果：' + String(event.data))); }
      };
      thread.onerror = (): void => { reject(new Error('动态编译工作线程异常')); };
      thread.postMessage(request);
    });
  }
  static async build(context: common.UIAbilityContext, project: string, report: BuildProgress, install: boolean = true): Promise<string> {
    if (OfflineProjectBuilder.running) throw new Error('已有构建任务正在执行');
    OfflineProjectBuilder.running = true;
    try { return await OfflineProjectBuilder.buildProject(context, project, report, install); }
    finally { OfflineProjectBuilder.running = false; }
  }
  static async installPrepared(context: common.UIAbilityContext, project: string, id: string, report: BuildProgress): Promise<string> {
    if (OfflineProjectBuilder.running) throw new Error('已有构建任务正在执行');
    if (!/^\d+$/.test(id)) throw new Error('构建记录无效，请重新编译');
    OfflineProjectBuilder.running = true;
    try {
      const parent = context.filesDir + '/offline-build';
      const text = fileIo.readTextSync(parent + '/' + id + '/result.json');
      const built = JSON.parse(text) as PreparedBuild;
      if (built.id !== id || built.project !== project) throw new Error('当前工程已切换，请重新编译');
      if (!built.signedHap.startsWith(context.filesDir + '/native-toolchain/') ||
        (await hash.hash(built.signedHap, 'sha256')).toLowerCase() !== built.signedHapSha256.toLowerCase()) {
        throw new Error('待安装 HAP 已变化，请重新编译；未安装其他构建产物');
      }
      report('> ArkForge :installPreparedHap ' + id);
      const independent = built.bundleName !== 'com.example.harmonyplayground';
      if (!independent) OfflineProjectBuilder.write(parent + '/pending-install.json', text);
      const installed = independent ? await LocalDevice.installSignedHap(context, built.signedHap, report) :
        await LocalDevice.installSignedDemo(context, report);
      if (independent) OfflineProjectBuilder.write(parent + '/latest.json', text);
      return installed;
    } finally { OfflineProjectBuilder.running = false; }
  }
  private static async buildProject(context: common.UIAbilityContext, project: string, report: BuildProgress, install: boolean): Promise<string> {
    const id = String(Date.now());
    const parent = context.filesDir + '/offline-build';
    const root = parent + '/' + id;
    OfflineProjectBuilder.write(context.filesDir + '/latest-sdk-diagnostics.json', JSON.stringify({ project: project, diagnostics: [] }));
    fileIo.mkdirSync(root + '/js', true);
    report('> ArkForge :validateHarmonyProject');
    if (!fileIo.accessSync(project + '/build-profile.json5')) throw new Error('缺少 HarmonyOS 工程 build-profile.json5');
    const plan = await ResourceCompiler.plan(context, project, report);
    report('> ArkForge :validateSigning');
    const signingFingerprint = await NativeToolchain.signingFingerprint(context, plan.bundleName);
    const independent = plan.bundleName !== 'com.example.harmonyplayground';
    let manifestText: string;
    if (fileIo.accessSync(project + '/.arkforge/offline.json')) manifestText = fileIo.readTextSync(project + '/.arkforge/offline.json');
    else manifestText = JSON.stringify({module:plan.moduleName,bundleName:plan.bundleName,abilityName:plan.abilityName,files:[]} as ResourceCache);
    const manifest = JSON.parse(manifestText) as ResourceCache;
    const moduleName = plan.moduleName;
    const bundleName = plan.bundleName;
    if (!/^[A-Za-z0-9_]+$/.test(moduleName) || !/^[A-Za-z0-9_.]+$/.test(bundleName)) throw new Error('无效模块或 bundle 名称');
    const modulePath = project + '/' + moduleName + '/src/main/module.json5';
    if (!fileIo.accessSync(modulePath)) throw new Error('当前工程需要对应模块的离线构建缓存；缺少 ' + moduleName + '/src/main/module.json5');
    report('> ArkForge project ' + bundleName + '/' + moduleName);
    manifest.files = manifest.files.filter((item: ResourceFile) => !item.path.includes('/resources/') && !item.path.startsWith('AppScope/') && !item.path.endsWith('.json5'));
    const resourceInputs = await ResourceCompiler.fingerprint(project, moduleName);
    const resourceTool = await context.resourceManager.getRawFileContent('compiler/resource-tool.json');
    const resourceCompilerSha = new util.TextDecoder().decodeToString(resourceTool);
    const started = Date.now();
    for (let offset = 0; offset < manifest.files.length; offset += 16) {
    await Promise.all(manifest.files.slice(offset, offset + 16).map(async (item: ResourceFile): Promise<void> => {
      if (item.path.startsWith('/') || item.path.split('/').includes('..')) throw new Error('无效缓存输入路径');
      const path = project + '/' + item.path;
      if (!fileIo.accessSync(path) || (await hash.hash(path, 'sha256')).toLowerCase() !== item.sha256) {
        throw new Error('资源或模块配置已改变，需要重新生成资源缓存：' + item.path);
      }
    }));
      report('> Verify resources/native inputs ' + Math.min(offset + 16, manifest.files.length) + '/' + manifest.files.length);
    }
    report('> ArkForge :resources/native UP-TO-DATE (' + (Date.now() - started) + ' ms；逐项核对缓存输入)');
    const bundle = root + '/ui-transform.bundle.js';
    await OfflineProjectBuilder.raw(context, 'compiler/ui-transform.bundle.js', bundle);
    const files: string[] = [];
    const sourceRoot = project + '/' + moduleName + '/src/main/ets';
    OfflineProjectBuilder.collect(sourceRoot, '', files);
    files.sort();
    if (files.length === 0) throw new Error('工程没有 ArkTS 源码');
    const sources: SourceEvidence[] = [];
    for (const path of files) sources.push({ path: moduleName + '/src/main/ets/' + path,
      sha256: await hash.hash(sourceRoot + '/' + path, 'sha256') });
    const compilerSha256 = await hash.hash(bundle, 'sha256');
    const checkerBundle = await SDKProjectChecker.stage(context);
    const checkInputData = SDKProjectChecker.input(project, moduleName, bundleName, files);
    const checkInput = JSON.stringify(checkInputData);
    OfflineProjectBuilder.write(root + '/sdk-input.json', checkInput);
    const sdkInputKey = await hash.hash(checkerBundle, 'sha256') + ':' + await hash.hash(root + '/sdk-input.json', 'sha256');
    OfflineProjectBuilder.write(root + '/resource-manifest.json', manifestText + '\n' + resourceInputs + '\n' + resourceCompilerSha);
    const resourceManifestSha256 = await hash.hash(root + '/resource-manifest.json', 'sha256');
    let cached: CachedBuild | undefined = undefined;
    let resourceCached: CachedBuild | undefined = undefined;
    for (const checkpoint of [parent + '/compiled.json', parent + '/latest.json']) {
    try {
      const previous = JSON.parse(fileIo.readTextSync(checkpoint)) as CachedBuild;
      if (previous.project === project && previous.resourceManifestSha256 === resourceManifestSha256 &&
        previous.root.startsWith(parent + '/') && !previous.root.includes('..') &&
        previous.resourceArchive && previous.resourceArchive.startsWith(parent + '/') && !previous.resourceArchive.includes('..') &&
        previous.resourceSymbols && fileIo.accessSync(previous.resourceArchive) &&
        await hash.hash(previous.resourceArchive, 'sha256') === previous.resourceArchiveSha256) resourceCached = previous;
      if (previous.project === project && previous.sdkChecked === true && previous.sdkInputKey === sdkInputKey && previous.compilerSha256 === compilerSha256 &&
        previous.resourceManifestSha256 === resourceManifestSha256 && previous.root.startsWith(parent + '/') &&
        !previous.root.includes('..') && previous.sources.length === sources.length &&
        previous.sources.every((item: SourceEvidence, index: number) => item.path === sources[index].path && item.sha256 === sources[index].sha256) &&
        fileIo.accessSync(previous.root + '/modules.abc') &&
        await hash.hash(previous.root + '/modules.abc', 'sha256') === previous.abcSha256 && previous.resourceArchive !== undefined &&
        fileIo.accessSync(previous.resourceArchive) && await hash.hash(previous.resourceArchive, 'sha256') === previous.resourceArchiveSha256) { cached = previous; break; }
    } catch (_) { /* Missing or invalid previous output requires real compilation. */ }
    }
    const list: string[] = [];
    const thread = new worker.ThreadWorker('entry/ets/workers/DynamicCompilerWorker.ets');
    let transformCanceled = false;
    let parallelTransform: Promise<WorkerReply> | undefined = undefined;
    try {
      let nativeArchive = '';
      if (manifest.cacheSha256 && fileIo.accessSync(project + '/.arkforge/resources.zip')) {
        nativeArchive = project + '/.arkforge/resources.zip';
        if ((await hash.hash(nativeArchive, 'sha256')).toLowerCase() !== manifest.cacheSha256) throw new Error('原生库输入包哈希不一致');
      }
      const resourceStarted = Date.now();
      const reusableResources = cached || resourceCached;
      const builtResources: BuiltResources = reusableResources ? { archive: reusableResources.resourceArchive || '', symbols: reusableResources.resourceSymbols || {} } :
        await ResourceCompiler.build(context, project, root, plan, report, nativeArchive);
      report('> ArkForge :resources ' + (reusableResources ? 'UP-TO-DATE' : 'FINISHED') + ' after ' + (Date.now() - resourceStarted) + ' ms');
      manifest.resources = builtResources.symbols;
      const resourceArchive = builtResources.archive;
      if (!cached && independent) {
        const transformProject = async (): Promise<WorkerReply> => {
          const virtualFiles: Record<string, string> = {};
          for (const path of files) {
            const name = '/project/' + moduleName + '/src/main/ets/' + path;
            virtualFiles[name] = checkInputData.files[name];
          }
          const options: ProjectOptions = {moduleName:moduleName,bundleName:bundleName,
            normalized:true,resources:manifest.resources || {},files:virtualFiles};
          const transformStarted = Date.now();
          const webBundle = await ArkUIWebRuntime.stage(context, bundle, compilerSha256);
          if (transformCanceled) return {ok:false,error:'Compiler task canceled'};
          report('> ArkUI batch transform ' + files.length + ' files (official SDK · parallel ArkWeb)');
          const result = JSON.parse(await SDKWebRuntime.transform(webBundle, JSON.stringify(options),
            files.map((path: string) => '/project/' + moduleName + '/src/main/ets/' + path), report)) as WorkerReply;
          report('> ArkForge :transformArkUI FINISHED after ' + (Date.now() - transformStarted) + ' ms');
          return result;
        };
        parallelTransform = transformProject().catch((error: Error): WorkerReply => { return {ok:false,error:String(error)}; });
      }
      if (cached) report('> ArkForge :checkArkTS UP-TO-DATE (官方 SDK、源码、声明和构建配置哈希一致)');
      else {
        const startedCheck = Date.now();
        report('> ArkForge :checkArkTS (official SDK 26.0.0.851 serviceChecker)');
        const checked = JSON.parse(await SDKWebRuntime.check(checkerBundle, checkInput, report)) as SDKResult;
        OfflineProjectBuilder.write(root + '/sdk-check-result.json', JSON.stringify(checked));
        OfflineProjectBuilder.write(context.filesDir + '/latest-sdk-diagnostics.json', JSON.stringify({ project: project, diagnostics: checked.diagnostics || [] }));
        // Text is emitted by the unchanged official SDK logger; metadata travels separately.
        if (checked.output) report(checked.output);
        if (checked.timing) report('> SDK phases: service ' + checked.timing.serviceMs + ' ms, diagnostic metadata ' + checked.timing.diagnosticsMs + ' ms');
        if (checked.hashCache) report('> SDK SHA-256 cache: ' + checked.hashCache.hits + ' hits, ' + checked.hashCache.misses + ' computed');
        if (!checked.ok) throw new Error('官方 SDK 检查失败，已停止构建；不会打包、签名或安装。' + (checked.error || ''));
        report('> ArkForge :checkArkTS PASSED after ' + (Date.now() - startedCheck) + ' ms');
      }
      const abc = root + '/modules.abc';
      if (cached) {
        report('> ArkUI/ABC UP-TO-DATE (全部源码、转换器、资源清单及 ABC 哈希一致)');
        fileIo.copyFileSync(cached.root + '/modules.abc', abc);
        if (fileIo.accessSync(cached.root + '/source-maps.json')) fileIo.copyFileSync(cached.root + '/source-maps.json', root + '/source-maps.json');
      } else {
      let batchOutputs: TransformedFile[] = [];
      const maps: SourceMapEntry[] = [];
      fileIo.mkdirSync(root + '/maps', true);
      if (independent) {
        const batch: WorkerReply = parallelTransform ? await parallelTransform : {ok:false,error:'ArkUI task was not created'};
        if (!batch.ok || !batch.outputs || batch.outputs.length !== files.length) {
          for (const diagnostic of batch.errors || []) report((batch.error || moduleName) + ':' + diagnostic.line + ':' + diagnostic.column + ': error: ' + diagnostic.message);
          throw new Error('批量 ArkUI 转换失败：' + (batch.error || JSON.stringify(batch.errors)));
        }
        batchOutputs = batch.outputs;
      }
      for (let index = 0; index < files.length; index++) {
        const path = files[index];
        const input = sourceRoot + '/' + path;
        if (!independent) report('> ArkUI transform [' + (index + 1) + '/' + files.length + '] ' + moduleName + '/src/main/ets/' + path);
        const transformed: WorkerReply = independent ? { ok: true, output: batchOutputs[index].output } :
          await OfflineProjectBuilder.request(thread, { kind: 'transform', bundlePath: bundle,
            source: fileIo.readTextSync(input), fileName: '/project/' + moduleName + '/src/main/ets/' + path, args: [] });
        if (!transformed.ok || !transformed.output) {
          if (transformed.errors) {
            for (const diagnostic of transformed.errors) report(moduleName + '/src/main/ets/' + path + ':' +
              diagnostic.line + ':' + diagnostic.column + ': error: ' + diagnostic.message);
          }
          throw new Error('ArkUI 转换失败 ' + path + ': ' + (transformed.error || JSON.stringify(transformed.errors)));
        }
        const javascript = root + '/js/' + index + '.js';
        OfflineProjectBuilder.write(javascript, transformed.output);
        const emittedMap = batchOutputs[index];
        if (emittedMap && emittedMap.sourceMap && emittedMap.generatedLineMap) {
          const mapFile = root + '/maps/' + index + '.json';
          const map: SourceMapFile = {sourceMap:emittedMap.sourceMap,generatedLineMap:emittedMap.generatedLineMap};
          OfflineProjectBuilder.write(mapFile, JSON.stringify(map));
          maps.push({path:moduleName + '/src/main/ets/' + path,javascript:javascript,mapFile:mapFile});
        }
        const record = independent ? '&' + moduleName + '/src/main/ets/' + path.replace(/\.(ets|ts)$/, '') + '&' :
          bundleName + '/' + moduleName + '/ets/' + path.replace(/\.(ets|ts)$/, '');
        list.push(javascript + ';' + record + ';esm;' + moduleName + '|' + moduleName + '|1.0.0|src/main/ets/' +
          path + ';' + moduleName + ';false;ets');
      }
      const inputList = root + '/filesInfo.txt';
      OfflineProjectBuilder.write(root + '/source-maps.json', JSON.stringify(maps));
      OfflineProjectBuilder.write(inputList, list.join('\n') + '\n');
      const args: string[] = ['es2abc', '--merge-abc', '--target-api-version=23', '--opt-level=0',
        '--file-threads=1', '--function-threads=1', '--debug-info', '--output', abc, '@' + inputList];
      report('> ' + args.join(' '));
      const compileStarted = Date.now();
      const compiled = await OfflineProjectBuilder.request(thread,
        { kind: 'compile', bundlePath: '', source: '', fileName: '', args: args });
      report('> ArkForge :compileABC FINISHED after ' + (Date.now() - compileStarted) + ' ms');
      if (!compiled.ok || !fileIo.accessSync(abc)) throw new Error('方舟动态编译失败：' + JSON.stringify(compiled));
      }
      for (const item of sources) {
        if (await hash.hash(project + '/' + item.path, 'sha256') !== item.sha256) throw new Error('构建期间源码发生变化，请重新构建：' + item.path);
      }
      const currentFiles: string[] = [];
      OfflineProjectBuilder.collect(sourceRoot, '', currentFiles);
      currentFiles.sort();
      if (currentFiles.join('\n') !== files.join('\n') ||
        JSON.stringify(SDKProjectChecker.input(project, moduleName, bundleName, currentFiles)) !== checkInput) {
        throw new Error('构建期间工程输入发生变化，请重新构建；本次不会安装。');
      }
      if (await ResourceCompiler.fingerprint(project, moduleName) !== resourceInputs) throw new Error('构建期间资源、配置或原生依赖发生变化，本次不会安装');
      const compiledCheckpoint: CachedBuild = {project:project,root:root,sources:sources,
        abcSha256:await hash.hash(abc,'sha256'),compilerSha256:compilerSha256,resourceManifestSha256:resourceManifestSha256,
        sdkInputKey:sdkInputKey,sdkChecked:true,resourceArchive:resourceArchive,
        resourceArchiveSha256:await hash.hash(resourceArchive,'sha256'),resourceSymbols:builtResources.symbols};
      // A signing/install failure may reuse validated compilation, without promoting an installed result.
      OfflineProjectBuilder.write(parent + '/compiled.json', JSON.stringify(compiledCheckpoint));
      const hap = root + '/' + moduleName + '-unsigned.hap';
      const signedHap = NativeToolchain.signedPath(context, bundleName);
      const reuseSigned = cached !== undefined && cached.signingFingerprint === signingFingerprint &&
        cached.signedHap === signedHap && cached.hapSha256 !== undefined && fileIo.accessSync(signedHap) &&
        await hash.hash(signedHap, 'sha256') === cached.signedHapSha256;
      if (reuseSigned) {
        report('> ArkForge :packageHap/:signHap UP-TO-DATE (源码、资源、签名材料及已验签 HAP 哈希一致；继续执行系统安装)');
      } else {
      report('> ArkForge :packageHap (本机资源编译输出 + ABC + 已校验原生库)');
      const packaged = await OfflineProjectBuilder.request(thread,
        { kind: 'pack', bundlePath: '', source: '', fileName: '', args: [resourceArchive, abc, hap] });
      if (!packaged.ok) throw new Error('HAP 打包失败：' + packaged.error);
      report('> ArkForge :signHap');
      await NativeToolchain.signDemo(context, report, hap, bundleName);
      }
      const result = JSON.stringify({ id: id, project: project, root: root, sources: sources,
        compilerSha256: compilerSha256, resourceManifestSha256: resourceManifestSha256, reusedAbc: cached !== undefined,
        signingFingerprint: signingFingerprint, reusedSignedHap: reuseSigned,
        sdkChecked: true, sdkInputKey: sdkInputKey,
        resourceArchive: resourceArchive, resourceArchiveSha256: await hash.hash(resourceArchive, 'sha256'), resourceSymbols: builtResources.symbols,
        mode: 'offline-incremental', bundleName: bundleName, moduleName: moduleName,
        abilityName: plan.abilityName || 'EntryAbility', signedHap: signedHap,
        abcSha256: await hash.hash(abc, 'sha256'),
        hapSha256: reuseSigned ? cached?.hapSha256 : await hash.hash(hap, 'sha256'),
        signedHapSha256: await hash.hash(signedHap, 'sha256') });
      OfflineProjectBuilder.write(root + '/result.json', result);
      OfflineProjectBuilder.write(parent + '/compiled.json', result);
      if (!install) {
        report('> Prepared build: ' + id);
        return '当前工程编译、打包、签名和验签完成，等待确认安装。';
      }
      // Self-update terminates this process; independent applications promote
      // their result only after the package manager confirms installation.
      if (!independent) OfflineProjectBuilder.write(parent + '/pending-install.json', result);
      report('> Build evidence: ' + root + '/result.json');
      const installed = independent ? await LocalDevice.installSignedHap(context, signedHap, report) :
        await LocalDevice.installSignedDemo(context, report);
      if (independent) OfflineProjectBuilder.write(parent + '/latest.json', result);
      return installed;
    } finally { transformCanceled = true; SDKWebRuntime.cancelTransform(); thread.terminate(); }
  }
}

interface AppConfig { app: AppIdentity; }
interface AppIdentity { bundleName: string; }
interface ModuleConfig { module: ModuleIdentity; }
interface ModuleIdentity { name: string; }
interface ProjectOptions {
  moduleName: string;
  bundleName: string;
  normalized: boolean;
  resources: Record<string, Record<string, Record<string, number>>>;
  files: Record<string, string>;
}
