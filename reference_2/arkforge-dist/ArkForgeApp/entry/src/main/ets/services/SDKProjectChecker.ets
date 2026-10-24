import { SDKWebRuntime } from './SDKWebRuntime';
import { common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { BuildProgress } from './BuildConsole';
import { AtomicFiles } from './AtomicFiles';

interface CheckerManifest { sha256: string; sdkVersion: string; }
interface WebCheckerManifest { source: string; sha256: string; wrapper: number; }
export interface SDKInput { moduleName: string; bundleName: string; files: Record<string, string>; operation?: string; }
export interface SDKDiagnostic { fileName: string; line: number; column: number; code: number; category: number; message: string; }
export interface SDKResult { ok: boolean; diagnostics?: SDKDiagnostic[]; output?: string; error?: string; sdkVersion?: string; sourceFiles?: number; }

export class SDKProjectChecker {
  static async verify(context: common.UIAbilityContext, report: BuildProgress): Promise<string> {
    const bundle = await SDKProjectChecker.stage(context);
    const results: SDKResult[] = [];
    const kinds: string[] = ['valid', 'type', 'name', 'rule', 'syntax'];
      for (const kind of kinds) {
        report('> SDK self-check ' + kind);
        const type = kind === 'rule' ? 'any' : 'number';
        const value = kind === 'type' ? '"wrong"' : (kind === 'syntax' ? '' : '1');
        const body = kind === 'name' ? 'Text(missingValue)' : 'Text(this.count.toString())';
        const files: Record<string, string> = {};
        files['/project/entry/src/main/ets/Probe.ets'] = '@Entry\n@Component\nstruct Probe {\n @State count: ' + type + ' = ' + value + ';\n build(){' + body + '}\n}';
        const input: SDKInput = { moduleName: 'entry', bundleName: 'com.arkforge.sdkcheck', files: files };
        const checked = JSON.parse(await SDKWebRuntime.check(bundle, JSON.stringify(input), report)) as SDKResult;
        results.push(checked);
        const code = kind === 'type' ? 2322 : (kind === 'name' ? 2304 : (kind === 'rule' ? 8 : 1005));
        const expected = kind === 'valid' ? checked.ok : !checked.ok &&
          (checked.diagnostics || []).some((item: SDKDiagnostic) => item.category === 1 && (kind === 'syntax' ? item.line === 4 : item.code === code));
        if (!expected) throw new Error('SDK 自检不符合预期 ' + kind + ': ' + JSON.stringify(checked));
        report('> SDK self-check ' + kind + ' PASSED');
      }
      const path = context.filesDir + '/sdk-check/self-test.json';
      const file = fileIo.openSync(path, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      try { fileIo.writeSync(file.fd, JSON.stringify({ allPassed: true, kinds: kinds, results: results, checkerSha256: await hash.hash(bundle, 'sha256') })); }
      finally { fileIo.closeSync(file); }
      return '官方 SDK 语法、类型、名称解析和 ArkTS 规则自检通过。\nC++ 修改需要对应原生编译工具链；不会将已有原生库冒充重新编译的产物。';
  }

  static async stage(context: common.UIAbilityContext, rebuildWrapper: boolean = false): Promise<string> {
    const bytes = await context.resourceManager.getRawFileContent('compiler/sdk-check.json');
    const manifest = JSON.parse(new util.TextDecoder().decodeToString(bytes)) as CheckerManifest;
    const directory = context.filesDir + '/sdk-check';
    if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory, true);
    const path = directory + '/sdk-check.bundle.js';
    if (fileIo.accessSync(path) && (await hash.hash(path, 'sha256')).toLowerCase() === manifest.sha256) { await SDKProjectChecker.stageWeb(path, manifest.sha256, rebuildWrapper); return path; }
    const bundle = await context.resourceManager.getRawFileContent('compiler/sdk-check.bundle.js');
    const file = fileIo.openSync(path, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, bundle.buffer); } finally { fileIo.closeSync(file); }
    if ((await hash.hash(path, 'sha256')).toLowerCase() !== manifest.sha256) throw new Error('官方 SDK 检查包校验失败');
    await SDKProjectChecker.stageWeb(path, manifest.sha256);
    return path;
  }

  static async benchmarkStaging(context: common.UIAbilityContext): Promise<string> {
    // Both paths verify the original SDK hash. No project input or checker is skipped.
    const started = Date.now();
    const path = await SDKProjectChecker.stage(context, true);
    const rewritten = Date.now();
    await SDKProjectChecker.stage(context);
    return JSON.stringify({rewriteMs:rewritten-started,reuseMs:Date.now()-rewritten,bytes:fileIo.statSync(path).size,scope:'SDK staging only'});
  }

  private static async stageWeb(path: string, sourceSha: string, rebuild: boolean = false): Promise<void> {
    const target = path.replace('sdk-check.bundle.js', 'sdk-check.web.js');
    const metadata = target + '.json';
    try {
      const previous = JSON.parse(fileIo.readTextSync(metadata)) as WebCheckerManifest;
      if (!rebuild && previous.source === sourceSha && previous.wrapper === 1 && fileIo.accessSync(target) &&
        await hash.hash(target, 'sha256') === previous.sha256) return;
    } catch (_) { /* Missing or corrupt generated wrapper must be rebuilt. */ }
    const source = fileIo.readTextSync(path);
    const marker = 'JSON.stringify(__load("/checker.cjs").check(__arkforgeOptions));';
    if (!source.endsWith(marker)) throw new Error('Unsupported SDK bundle wrapper');
    const file = fileIo.openSync(target, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, 'try {\n' + source.substring(0, source.length - marker.length) + 'globalThis.__arkforgeCheckResult=' + marker +
      '\n} catch(error) { globalThis.__arkforgeCheckResult=JSON.stringify({ok:false,error:String(error)}); }'); }
    finally { fileIo.closeSync(file); }
    const generated: WebCheckerManifest = {source:sourceSha,sha256:await hash.hash(target,'sha256'),wrapper:1};
    AtomicFiles.write(metadata,JSON.stringify(generated));
  }

  private static add(project: string, relative: string, files: Record<string, string>): void {
    const path = project + '/' + relative;
    if (fileIo.accessSync(path) && fileIo.statSync(path).isFile()) files['/project/' + relative] = fileIo.readTextSync(path);
  }

  private static declarations(project: string, relative: string, files: Record<string, string>): void {
    const root = project + '/' + relative;
    if (!fileIo.accessSync(root)) return;
    for (const name of fileIo.listFileSync(root)) {
      if (['.git', '.hvigor', 'build', 'node_modules'].includes(name)) continue;
      const next = relative + '/' + name;
      if (fileIo.statSync(project + '/' + next).isDirectory()) SDKProjectChecker.declarations(project, next, files);
      else if (/\.d\.(ts|ets)$/.test(name) || ['oh-package.json5', 'package.json'].includes(name)) SDKProjectChecker.add(project, next, files);
    }
  }

  static input(project: string, moduleName: string, bundleName: string, sources: string[]): SDKInput {
    const files: Record<string, string> = {};
    for (const path of sources) SDKProjectChecker.add(project, moduleName + '/src/main/ets/' + path, files);
    for (const path of ['build-profile.json5', 'oh-package.json5', 'AppScope/app.json5',
      moduleName + '/build-profile.json5', moduleName + '/oh-package.json5', moduleName + '/src/main/module.json5']) {
      SDKProjectChecker.add(project, path, files);
    }
    SDKProjectChecker.declarations(project, moduleName + '/src/main/cpp/types', files);
    SDKProjectChecker.declarations(project, moduleName + '/src/main/ets', files);
    SDKProjectChecker.declarations(project, moduleName + '/oh_modules', files);
    SDKProjectChecker.declarations(project, 'oh_modules', files);
    return { moduleName: moduleName, bundleName: bundleName, files: files };
  }
}
