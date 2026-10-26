import { common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import deviceInfo from '@ohos.deviceInfo';
import { SourceMaps, SourcePosition, SourceMapEntry } from './SourceMaps';

interface CompilerConfiguration { partialUpdateMode: boolean; optimizeComponent: boolean; }
interface CompilerMetadata { sdkVersion: string; sha256: string; partialUpdateConfig: CompilerConfiguration; }
interface BuiltSource { path: string; sha256: string; }
interface RuntimeBuild { id: string; bundleName: string; root: string; project: string; sources?: BuiltSource[]; compilerSha256?: string; }
interface RuntimeFrame {
  generatedUrl: string; generatedLine: number; generatedColumn: number;
  original?: SourcePosition; sourceMatchesBuild?: boolean; sourceExcerpt?: string; generatedExcerpt?: string;
}
interface RuntimeFault { file: string; reason: string; report: string; frames: RuntimeFrame[]; predatesBuild: boolean; }
export interface RuntimeReport {
  bundleName: string; runningPid: string; deviceApi: number; deviceType: string;
  faultCollectionOk: boolean; compiler?: CompilerMetadata; installedCompilerMatches?: boolean;
  lastArkForgeBuild: string; faults: RuntimeFault[]; processLog: string; collectionNotes: string[];
}

/** Official FaultLog transport also works after the target process has exited. */
export class RuntimeDiagnostics {
  static redact(text: string): string {
    return text.replace(/sk-[A-Za-z0-9_-]{20,}/g, '[API_KEY]')
      .replace(/(Bearer\s+)[A-Za-z0-9._-]+/gi, '$1[REDACTED]')
      .replace(/(pairing token\s+)[A-Za-z0-9_-]+/gi, '$1[REDACTED]');
  }

  private static excerpt(text: string, line: number): string {
    return text.split('\n').slice(Math.max(0, line - 5), line + 5)
      .map((value: string, index: number) => String(Math.max(1, line - 4) + index) + ': ' + value).join('\n').slice(0,1800);
  }

  private static async frames(text: string, built: RuntimeBuild): Promise<RuntimeFrame[]> {
    const frames: RuntimeFrame[] = [];
    const seen: string[] = [];
    const maps = new SourceMaps(built.root);
    let entries: SourceMapEntry[] = [];
    try { entries = JSON.parse(fileIo.readTextSync(built.root + '/source-maps.json')) as SourceMapEntry[]; } catch (_) {}
    const pattern = /\(([^()\n]+?\.(?:ets|ts|js)):(\d+):(\d+)\)/g;
    let match: RegExpExecArray | null;
    while ((match = pattern.exec(text)) !== null && frames.length < 3) {
      if (!match[1].includes('|') || seen.includes(match[1])) continue;
      seen.push(match[1]);
      const frame: RuntimeFrame = {generatedUrl:match[1],generatedLine:Number(match[2]),generatedColumn:Number(match[3])};
      try { frame.original = maps.original(match[1],Number(match[2])-1,Number(match[3])-1); } catch (_) {}
      const path = SourceMaps.path(match[1]);
      const entry = entries.find((item: SourceMapEntry) => item.path === path);
      if (entry && fileIo.accessSync(entry.javascript)) {
        frame.generatedExcerpt = RuntimeDiagnostics.excerpt(fileIo.readTextSync(entry.javascript),frame.generatedLine);
      }
      if (frame.original) {
        const source = built.sources?.find((item: BuiltSource) => item.path === frame.original?.path);
        const physical = built.project + '/' + frame.original.path;
        if (source && fileIo.accessSync(physical)) {
          frame.sourceMatchesBuild = (await hash.hash(physical,'sha256')).toUpperCase() === source.sha256.toUpperCase();
          frame.sourceExcerpt = RuntimeDiagnostics.excerpt(fileIo.readTextSync(physical),frame.original.line);
        }
      }
      frames.push(frame);
    }
    return frames;
  }

  static async collect(context: common.UIAbilityContext, run: (command: string) => Promise<string>, project: string = ''): Promise<RuntimeReport> {
    const manifest = context.filesDir + '/offline-build/latest.json';
    if (!fileIo.accessSync(manifest)) throw new Error('请先安装当前工程，再收集该应用的运行诊断');
    const built = JSON.parse(fileIo.readTextSync(manifest)) as RuntimeBuild;
    if (project && built.project !== project) throw new Error('请先构建安装当前工程，再读取对应应用的运行诊断');
    if (!/^[A-Za-z0-9_.]+$/.test(built.bundleName) || !built.root.startsWith(context.filesDir + '/offline-build/') ||
      built.root.includes('..')) throw new Error('请重新选择已构建安装的工程');
    const report: RuntimeReport = {bundleName:built.bundleName,runningPid:'',deviceApi:deviceInfo.sdkApiVersion,
      deviceType:deviceInfo.deviceType,lastArkForgeBuild:built.id,faultCollectionOk:false,faults:[],processLog:'',collectionNotes:[]};
    try {
      const bytes = await context.resourceManager.getRawFileContent('compiler/ui-transform.json');
      report.compiler = JSON.parse(new util.TextDecoder().decodeToString(bytes)) as CompilerMetadata;
      report.installedCompilerMatches = report.compiler.sha256.toUpperCase() === (built.compilerSha256 || '').toUpperCase();
    } catch (_) {}
    const pidText = await run('hdc -t 127.0.0.1:8710 shell echo ARKFORGE_PID=$(pidof ' + built.bundleName + ')');
    report.runningPid = /ARKFORGE_PID=([1-9]\d*)/.exec(pidText)?.[1] || '';
    if (report.runningPid) {
      report.processLog = RuntimeDiagnostics.redact((await run('hdc -t 127.0.0.1:8710 shell hilog -z 120 -P ' + report.runningPid)).slice(-5000));
    }
    const directory = context.tempDir + '/runtime-faults/' + built.bundleName;
    if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory,true);
    // HDC's documented FaultLog transfer supports this path even when shell ls
    // has no permission. Retain only this target's reports in the app cache.
    const transferred = await run('hdc -t 127.0.0.1:8710 file recv /data/log/faultlog/faultlogger "' + directory + '"');
    report.faultCollectionOk = transferred.includes('FileTransfer finish');
    if (!report.faultCollectionOk) report.collectionNotes.push('本次系统故障传输结果：' + transferred.slice(-500) + '。保留的历史报告可供参考。');
    const folder = fileIo.accessSync(directory + '/faultlogger') ? directory + '/faultlogger' : directory;
    const names: string[] = [];
    for (const name of fileIo.listFileSync(folder)) {
      const path = folder + '/' + name;
      if (!fileIo.lstatSync(path).isFile()) continue;
      if (/^(jscrash|cppcrash|appfreeze)-/.test(name) && name.includes('-' + built.bundleName + '-')) names.push(name);
      else fileIo.unlinkSync(path);
    }
    names.sort((a: string,b: string) => b.slice(b.lastIndexOf('-')).localeCompare(a.slice(a.lastIndexOf('-'))));
    for (const name of names.slice(0,2)) {
      const raw = fileIo.readTextSync(folder + '/' + name);
      const stamp = /-(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})(\d{3})\.log$/.exec(name);
      const happened = stamp ? new Date(Number(stamp[1]),Number(stamp[2])-1,Number(stamp[3]),Number(stamp[4]),Number(stamp[5]),Number(stamp[6]),Number(stamp[7])).getTime() : 0;
      const predates = happened === 0 || happened < Number(built.id);
      report.faults.push({file:name,predatesBuild:predates,reason:/^Reason:(.*)$/m.exec(raw)?.[1]?.trim() || '',
        report:RuntimeDiagnostics.redact(raw.slice(0,6500)),frames:predates?[]:await RuntimeDiagnostics.frames(raw,built)});
    }
    for (const name of names.slice(2)) fileIo.unlinkSync(folder + '/' + name);
    if (!names.length) report.collectionNotes.push('当前缓存中尚无此应用的系统故障报告。传输结果：' + transferred.slice(-300));
    report.collectionNotes.push('faults 是历史故障，按文件时间排序；核对故障 PID、生成代码、构建编号及 sourceMatchesBuild，再决定修改位置。');
    report.collectionNotes.push('系统堆栈中的 .ets 行号可能是生成代码位置；original 才是本地 source map 还原的位置。');
    return report;
  }
}
