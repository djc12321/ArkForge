import { common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { worker, MessageEvents, util } from '@kit.ArkTS';
import { zlib } from '@kit.BasicServicesKit';
import { SDKProjectChecker } from './SDKProjectChecker';
import { SDKWebRuntime } from './SDKWebRuntime';
import resources from 'libarkforge_resources.so';
import { BuildProgress } from './BuildConsole';
import { AtomicFiles } from './AtomicFiles';

interface ResourceReply { ok: boolean; status?: number; error?: string; }
export interface ProjectPlan { ok: boolean; error?: string; moduleName: string; bundleName: string; abilityName: string; configJSON: string; packInfoJSON: string; }
interface PackageContext { isSO: boolean; moduleName: string; bundleName: string; dependencyAlias: string; packageName: string; entryPath: string; version: string; }
export interface BuiltResources { archive: string; symbols: Record<string, Record<string, Record<string, number>>>; }
export class ResourceCompiler {
  static async plan(context: common.UIAbilityContext, project: string, report: BuildProgress): Promise<ProjectPlan> {
    const input = SDKProjectChecker.input(project, 'entry', '', []);
    input.operation = 'projectPlan';
    for (const name of fileIo.listFileSync(project)) {
      if (!fileIo.lstatSync(project + '/' + name).isDirectory()) continue;
      const path = project + '/' + name + '/src/main/module.json5';
      if (fileIo.accessSync(path)) input.files['/project/' + name + '/src/main/module.json5'] = fileIo.readTextSync(path);
    }
    const bundle = await SDKProjectChecker.stage(context);
    const plan = JSON.parse(await SDKWebRuntime.check(bundle, JSON.stringify(input), report)) as ProjectPlan;
    if (!plan.ok) throw new Error('工程配置解析失败：' + plan.error);
    return plan;
  }
  static collect(project: string, relative: string, result: string[]): void {
    if (!fileIo.accessSync(project + '/' + relative)) return;
    const stat = fileIo.lstatSync(project + '/' + relative);
    if (stat.isSymbolicLink()) throw new Error('构建输入不支持符号链接：' + relative);
    if (stat.isFile()) { result.push(relative); return; }
    for (const name of fileIo.listFileSync(project + '/' + relative).sort()) ResourceCompiler.collect(project, relative + '/' + name, result);
  }
  static async fingerprint(project: string, moduleName: string): Promise<string> {
    const files: string[] = [];
    for (const path of ['AppScope', moduleName + '/src/main/resources', moduleName + '/src/main/module.json5',
      moduleName + '/libs', moduleName + '/build-profile.json5', 'build-profile.json5']) ResourceCompiler.collect(project, path, files);
    const hashes: string[] = [];
    for (const path of files) hashes.push(path + ':' + await hash.hash(project + '/' + path, 'sha256'));
    return hashes.join('\n');
  }
  private static copy(source: string, destination: string): void {
    if (!fileIo.accessSync(source)) return;
    const stat = fileIo.lstatSync(source);
    if (stat.isSymbolicLink()) throw new Error('不支持符号链接原生依赖');
    if (stat.isFile()) { fileIo.copyFileSync(source, destination); return; }
    if (!fileIo.accessSync(destination)) fileIo.mkdirSync(destination, true);
    for (const name of fileIo.listFileSync(source)) ResourceCompiler.copy(source + '/' + name, destination + '/' + name);
  }
  static async build(context: common.UIAbilityContext, project: string, root: string, plan: ProjectPlan, report: BuildProgress, nativeArchive: string = ''): Promise<BuiltResources> {
    const output = root + '/resources';
    fileIo.mkdirSync(output, true);
    AtomicFiles.write(root + '/module.json', plan.configJSON);
    const ids = await context.resourceManager.getRawFileContent('compiler/id_defined.json');
    const file = fileIo.openSync(root + '/id_defined.json', fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, ids.buffer); } finally { fileIo.closeSync(file); }
    const args = [root + '/restool', '-i', project + '/AppScope', '-i', project + '/' + plan.moduleName + '/src/main',
      '-j', root + '/module.json', '-o', output, '-r', root + '/ResourceTable.txt', '-p', plan.bundleName, '--thread', '2'];
    await ResourceCompiler.run(args, report);
    const app: Record<string, Record<string, number>> = {};
    for (const line of fileIo.readTextSync(root + '/ResourceTable.txt').split(/\r?\n/)) {
      const parts = line.trim().split(/\s+/);
      if (parts.length !== 3) continue;
      if (!app[parts[0]]) app[parts[0]] = {};
      app[parts[0]][parts[1]] = Number(parts[2]);
    }
    const sysBytes = await context.resourceManager.getRawFileContent('compiler/system-resources.json');
    const sys = JSON.parse(new util.TextDecoder().decodeToString(sysBytes)) as Record<string, Record<string, number>>;
    if (nativeArchive) {
      resources.validateArchive(nativeArchive);
      const nativeInput = root + '/native-input';
      fileIo.mkdirSync(nativeInput);
      await zlib.decompressFile(nativeArchive, nativeInput);
      // Legacy projects may only contain their built C++ library in the imported SDK cache.
      // Copy native libraries only; resources/module.json always come from this compilation.
      ResourceCompiler.copy(nativeInput + '/libs', output + '/libs');
    }
    ResourceCompiler.copy(project + '/' + plan.moduleName + '/libs', output + '/libs');
    const contexts: Record<string, PackageContext> = {};
    contexts[plan.moduleName] = {isSO:false,moduleName:'',bundleName:'',dependencyAlias:'',packageName:plan.moduleName,entryPath:'src/main/',version:''};
    const libraries: string[] = [];
    ResourceCompiler.collect(output, 'libs', libraries);
    for (const path of libraries) {
      if (!path.endsWith('.so')) continue;
      const name = path.substring(path.lastIndexOf('/') + 1);
      contexts[name] = {isSO:true,moduleName:'',bundleName:'',dependencyAlias:'',packageName:name,entryPath:'index',version:''};
    }
    AtomicFiles.write(output + '/pkgContextInfo.json', JSON.stringify(contexts));
    AtomicFiles.write(output + '/pack.info', plan.packInfoJSON);
    const archive = root + '/resources.zip';
    await zlib.compressFile(output, archive, {});
    const symbols: Record<string, Record<string, Record<string, number>>> = {};
    symbols['app'] = app; symbols['sys'] = sys;
    return { archive: archive, symbols: symbols };
  }
  static async run(args: string[], report: BuildProgress): Promise<void> {
    report('> ' + args.join(' '));
    const thread = new worker.ThreadWorker('entry/ets/workers/DynamicCompilerWorker.ets');
    try {
      const result = await new Promise<ResourceReply>((resolve, reject) => {
        thread.onmessage = (event: MessageEvents): void => { resolve(JSON.parse(event.data as string) as ResourceReply); };
        thread.onerror = (): void => { reject(new Error('资源编译工作线程异常')); };
        thread.postMessage({ kind: 'resource', args: args, source: '', fileName: '', bundlePath: '' });
      });
      if (!result.ok) throw new Error('资源编译失败：' + (result.error || result.status));
    } finally { thread.terminate(); }
  }
  static async verify(context: common.UIAbilityContext, report: BuildProgress): Promise<string> {
    const root = context.filesDir + '/resource-check/' + Date.now();
    fileIo.mkdirSync(root + '/main/resources/base/element', true);
    AtomicFiles.write(root + '/main/resources/base/element/string.json', '{"string":[{"name":"app_name","value":"Device resource compiler"}]}');
    AtomicFiles.write(root + '/module.json', '{"app":{"bundleName":"com.arkforge.resourcecheck","versionCode":1,"versionName":"1.0.0","label":"$string:app_name"},"module":{"name":"entry","type":"entry","deviceTypes":["tablet"],"mainElement":"EntryAbility","abilities":[{"name":"EntryAbility","srcEntry":"./ets/entryability/EntryAbility.ets","label":"$string:app_name"}]}}');
    fileIo.mkdirSync(root + '/output');
    await ResourceCompiler.run(['restool','-i',root+'/main','-j',root+'/module.json','-o',root+'/output','-r',root+'/ResourceTable.txt','-p','com.arkforge.resourcecheck','--thread','2'], report);
    if (!fileIo.accessSync(root + '/output/resources.index')) throw new Error('资源编译未生成索引');
    report('> resource compiler PASSED: ' + root + '/output/resources.index');
    return '本机资源编译器自检通过';
  }
}
