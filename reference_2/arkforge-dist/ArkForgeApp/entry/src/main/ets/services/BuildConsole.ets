import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import native from 'libarkforge_script.so';
import { util } from '@kit.ArkTS';

export interface BuildLogEntry {
  id: string;
  time: string;
  level: string;
  text: string;
}

export type BuildProgress = (message: string) => void;

class LogTail {
  path: string;
  position: number = 0;
  inode: string = '';
  constructor(path: string, skipExisting: boolean = false) {
    this.path = path;
    if (skipExisting && fileIo.accessSync(path)) {
      this.inode = String(fileIo.statSync(path).ino);
      this.position = fileIo.statSync(path).size;
    }
  }
}

/** One chronological console for tool output, diagnostics, and task results. */
export class BuildConsole {
  private sequence: number = 0;
  private tails: LogTail[] = [];
  private timer: number = -1;
  private started: number = Date.now();
  private output: string;
  private events: string;
  private notify: (entry: BuildLogEntry) => void;

  constructor(context: common.UIAbilityContext, title: string, notify: (entry: BuildLogEntry) => void) {
    const directory = context.filesDir + '/build-logs';
    if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory, true);
    const prefix = directory + '/' + this.started;
    this.output = prefix + '.log';
    this.events = prefix + '.jsonl';
    this.notify = notify;
    const capture = prefix + '.native.log';
    this.tails.push(new LogTail(capture));
    this.tails.push(new LogTail(context.tempDir + '/hdc/hdc_out.txt', true));
    native.startToolLog(capture);
    this.appendFile(directory + '/latest.json', JSON.stringify({ output: this.output, events: this.events }), false);
    this.write('> ArkForge Started ' + title);
    this.write('日志文件：' + this.output);
    this.timer = setInterval(() => { this.poll(false); }, 200);
  }

  private appendFile(path: string, text: string, append: boolean = true): void {
    const mode = fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY |
      (append ? fileIo.OpenMode.APPEND : fileIo.OpenMode.TRUNC);
    const file = fileIo.openSync(path, mode);
    try { fileIo.writeSync(file.fd, text); } finally { fileIo.closeSync(file); }
  }

  static classify(text: string): string {
    if (/^.*:\d+:\d+: warning\b/i.test(text)) return 'warning';
    if (/\b(error|fatal|failed|failure)\b|错误|失败|异常/i.test(text)) return 'error';
    if (/\bwarn(ing)?\b|警告/i.test(text)) return 'warning';
    if (/BUILD SUCCESSFUL|> ArkForge Finished/.test(text)) return 'success';
    return 'info';
  }

  write(text: string, level: string = ''): void {
    // Never print passwords when displaying a tool command or diagnostic.
    const safe = text.replace(/\x1b\[[0-9;]*m/g, '').replace(/(-(?:keyPwd|keystorePwd|userPwd|storePassword|keyPassword)\s+)("[^"]*"|'[^']*'|\S+)/gi, '$1******');
    const entries: BuildLogEntry[] = [];
    for (const line of safe.replace(/\r\n/g, '\n').split('\n')) {
      if (!line) continue;
      // ArkWeb platform telemetry is retained in the raw native log, outside build diagnostics.
      if (/^\[\.\.\/.*\.(cc|cpp):\d+\]/.test(line)) continue;
      const entry: BuildLogEntry = { id: String(this.started) + '-' + (++this.sequence),
        time: new Date().toTimeString().slice(0, 8), level: level || BuildConsole.classify(line), text: line };
      entries.push(entry);
    }
    if (entries.length === 0) return;
    this.appendFile(this.output, entries.map((entry: BuildLogEntry): string => '[' + entry.time + '] ' + entry.text + '\n').join(''));
    this.appendFile(this.events, entries.map((entry: BuildLogEntry): string => JSON.stringify(entry) + '\n').join(''));
    for (const entry of entries) {
      hilog.info(0, 'ArkForgeBuild', '%{public}s %{public}s', entry.level, entry.text);
      this.notify(entry);
    }
  }

  private poll(final: boolean): void {
    for (const tail of this.tails) {
      try {
      if (!fileIo.accessSync(tail.path)) continue;
      const inode = String(fileIo.statSync(tail.path).ino);
      if (tail.inode !== inode) { tail.position = 0; tail.inode = inode; }
      const size = fileIo.statSync(tail.path).size;
      if (size < tail.position) tail.position = 0;
      if (size <= tail.position) continue;
      // Read only new bytes; stop at a complete UTF-8 line during live polling.
      const buffer = new ArrayBuffer(size - tail.position);
      const file = fileIo.openSync(tail.path, fileIo.OpenMode.READ_ONLY);
      let read = 0;
      try { read = fileIo.readSync(file.fd, buffer, { offset: tail.position }); }
      finally { fileIo.closeSync(file); }
      const bytes = new Uint8Array(buffer, 0, read);
      let end = read;
      if (!final) { while (end > 0 && bytes[end - 1] !== 10) end--; }
      if (end > 0) {
        this.write(new util.TextDecoder().decodeToString(bytes.slice(0, end)));
        tail.position += end;
      }
      } catch (_) {
        // HDC rotates its output file while a command starts; retry next tick.
      }
    }
  }

  finish(ok: boolean): void {
    if (this.timer >= 0) clearInterval(this.timer);
    native.stopToolLog();
    this.poll(true);
    this.write('> ArkForge Finished after ' + (Date.now() - this.started) + ' ms', ok ? 'success' : 'error');
    this.write(ok ? 'BUILD SUCCESSFUL' : 'BUILD FAILED', ok ? 'success' : 'error');
  }

  flush(): void { this.poll(true); }

  static restore(context: common.UIAbilityContext): BuildLogEntry[] {
    const latest = context.filesDir + '/build-logs/latest.json';
    if (!fileIo.accessSync(latest)) return [];
    const manifest = JSON.parse(fileIo.readTextSync(latest)) as ConsoleManifest;
    if (!fileIo.accessSync(manifest.events)) return [];
    const restored = new Array<BuildLogEntry>();
    for (const line of fileIo.readTextSync(manifest.events).split('\n')) {
      if (!line) continue;
      try { restored.push(JSON.parse(line) as BuildLogEntry); } catch (_) {
        // A process killed during self-update can leave an incomplete final row.
      }
    }
    return restored.slice(-500);
  }

  static recordRecovery(context: common.UIAbilityContext, text: string, ok: boolean): BuildLogEntry[] {
    const entries = BuildConsole.restore(context);
    const latest = context.filesDir + '/build-logs/latest.json';
    if (!fileIo.accessSync(latest)) return entries;
    const manifest = JSON.parse(fileIo.readTextSync(latest)) as ConsoleManifest;
    const entry: BuildLogEntry = { id: String(Date.now()) + '-recovery',
      time: new Date().toTimeString().slice(0, 8), level: ok ? 'success' : 'warning',
      text: text + (ok ? '\nBUILD SUCCESSFUL (system update confirmed after restart)' : '') };
    for (const path of [manifest.events, manifest.output]) {
      const file = fileIo.openSync(path, fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.APPEND);
      try { fileIo.writeSync(file.fd, path === manifest.events ? JSON.stringify(entry) + '\n' :
        '[' + entry.time + '] ' + entry.text + '\n'); } finally { fileIo.closeSync(file); }
    }
    return entries.concat(entry);
  }
}

interface ConsoleManifest {
  output: string;
  events: string;
}
