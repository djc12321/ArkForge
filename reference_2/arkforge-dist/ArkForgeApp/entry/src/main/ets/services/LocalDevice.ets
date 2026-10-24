import { AtomicFiles } from './AtomicFiles';
import hdc from 'libhdc_z.so';
import native from 'libarkforge_script.so';
import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { InstallJournal } from './InstallJournal';
import { BuildProgress } from './BuildConsole';

/** Local host transport talks to this device's debug daemon over loopback. */
export class LocalDevice {
  static async signingUdid(context: common.UIAbilityContext): Promise<string> {
    await LocalDevice.connect(context);
    const output = await LocalDevice.run(context,'hdc -t 127.0.0.1:8710 shell bm get --udid');
    const match = /\b[A-Fa-f0-9]{64}\b/.exec(output);
    if (!match) throw new Error('未能读取本设备 UDID，请确认本机调试连接已授权');
    return match[0];
  }
  private static started: boolean = false;
  private static busy: boolean = false;
  static async prepareDebugger(context: common.UIAbilityContext, bundle: string, ability: string): Promise<string> {
    if (!/^[A-Za-z0-9_.]+$/.test(bundle) || !/^[A-Za-z0-9_.]+$/.test(ability) || bundle === context.applicationInfo.name) throw new Error('调试目标无效');
    await LocalDevice.connect(context);
    const started = await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 shell aa start -b ' + bundle + ' -a ' + ability);
    if (!started.includes('start ability successfully')) throw new Error('调试启动失败：' + started);
    // Attach to the running application after restoring the editor. A startup
    // wait (-D) prevents that return on devices that serialize ability starts.
    await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 shell aa start -b com.example.harmonyplayground -a EntryAbility');
    const attached = await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 shell aa attach -b ' + bundle);
    if (!attached.includes('success')) throw new Error('调试附加失败：' + attached);
    // The embedded HDC output can also contain host stdout. Match our explicit
    // marker instead of accepting an unrelated numeric log line as a PID.
    const output = await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 shell echo ARKFORGE_PID=$(pidof ' + bundle + ')');
    AtomicFiles.write(context.tempDir + '/hdc/debug-pid.txt', output);
    const pid = /ARKFORGE_PID=([1-9]\d*)/.exec(output)?.[1];
    if (!pid) throw new Error('调试目标未创建进程：' + output.trim());
    return pid;
  }
  static async debugForward(context: common.UIAbilityContext, port: number, target: string, remove: boolean = false): Promise<void> {
    if (![9230, 9231].includes(port) || !/^ark:\d+@(Debugger|[A-Za-z0-9_.]+|\d+@Debugger)$/.test(target)) throw new Error('调试转发参数无效');
    const result = await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 fport ' + (remove ? 'rm ' : '') + 'tcp:' + port + ' ' + target);
    if (!remove && !result.includes('OK')) throw new Error('调试端口转发失败：' + result);
  }

  private static async run(context: common.UIAbilityContext, command: string, report?: BuildProgress): Promise<string> {
    if (LocalDevice.busy) throw new Error('本机 HDC 正在执行另一条命令');
    LocalDevice.busy = true;
    try {
      const loader = native.inspectLibrary('libhdc_z.so');
      if (loader !== 'loaded') throw new Error(loader);
      const directory = context.tempDir + '/hdc';
      if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory, true);
      if (!LocalDevice.started) {
        hdc.hdcStartServer(directory);
        LocalDevice.started = true;
        await new Promise<void>((resolve) => { setTimeout(resolve, 800); });
      }
      report?.('> ' + command);
      AtomicFiles.write(directory + '/last-command.txt', new Date().toISOString() + '\nSTART ' + command);
      return await new Promise<string>((resolve) => {
        hdc.hdcRun(command, directory, (output: string) => {
          AtomicFiles.write(directory + '/last-command.txt', new Date().toISOString() + '\nDONE ' + command + '\n' + output.slice(-4096));
          resolve(output);
        });
      });
    } finally {
      LocalDevice.busy = false;
    }
  }

  static async connect(context: common.UIAbilityContext, report?: BuildProgress): Promise<string> {
    const output = await LocalDevice.run(context, 'hdc tconn 127.0.0.1:8710', report);
    let targets = await LocalDevice.run(context, 'hdc list targets', report);
    // tconn can finish before the first local handshake registers a target.
    // Allow that handshake (and the system authorization dialog) to finish.
    for (let attempt = 0; attempt < 10 &&
      (targets.indexOf('127.0.0.1:8710') < 0 || targets.indexOf('Unauthorized') >= 0); attempt++) {
      if (attempt === 0) report?.('等待本机调试连接；若系统显示授权窗口，请允许。');
      await new Promise<void>((resolve) => { setTimeout(resolve, 1000); });
      targets = await LocalDevice.run(context, 'hdc list targets', report);
    }
    hilog.info(0x0000, 'ArkForgeNative', 'Local HDC targets: %{public}s', targets);
    if (targets.indexOf('127.0.0.1:8710') < 0 || targets.indexOf('Unauthorized') >= 0) {
      throw new Error('本机调试连接尚未授权：' + output + '\n' + targets);
    }
    return output + '\n' + targets;
  }

  static async installSignedDemo(context: common.UIAbilityContext, report?: BuildProgress): Promise<string> {
    const path = context.filesDir + '/native-toolchain/device-signed-demo.hap';
    if (!fileIo.accessSync(path)) throw new Error('请先完成本机 HAP 签名');
    await LocalDevice.connect(context, report);
    await InstallJournal.begin(context, path);
    report?.('安装同一应用会终止当前进程；重开 ArkForge 后将从系统更新时间恢复确认。');
    try {
      const output = await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 install -r "' + path + '"', report);
      const ok = output.indexOf('install bundle successfully') >= 0;
      hilog.info(0x0000, 'ArkForgeNative', 'Local HAP installation ok=%{public}s', String(ok));
      if (!ok) throw new Error(output);
      return output;
    } catch (error) {
      // An explicit failure must never become a success after a later, unrelated update.
      InstallJournal.fail(context);
      throw new Error((error as Error).message);
    }
  }

  static async readRuntimeLog(context: common.UIAbilityContext, report?: BuildProgress): Promise<string> {
    const built = JSON.parse(fileIo.readTextSync(context.filesDir + '/offline-build/latest.json')) as InstalledBuild;
    if (!/^[A-Za-z0-9_.]+$/.test(built.bundleName)) throw new Error('构建应用标识无效');
    await LocalDevice.connect(context, report);
    const output = await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 shell echo ARKFORGE_PID=$(pidof ' + built.bundleName + ')', report);
    const pid = /ARKFORGE_PID=([1-9]\d*)/.exec(output)?.[1];
    return await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 shell hilog -z 1000 ' +
      (pid ? '-P ' + pid : '-e ' + built.bundleName), report);
  }

  static async installSignedHap(context: common.UIAbilityContext, path: string, report?: BuildProgress): Promise<string> {
    if (!path.startsWith(context.filesDir + '/native-toolchain/') || path.includes('"')) throw new Error('无效安装路径');
    await LocalDevice.connect(context, report);
    const output = await LocalDevice.run(context, 'hdc -t 127.0.0.1:8710 install -r "' + path + '"', report);
    if (output.indexOf('install bundle successfully') < 0) throw new Error(output);
    report?.('独立应用安装成功；可启动其导出的入口 Ability。');
    return output;
  }
}

interface InstalledBuild { bundleName: string; }
