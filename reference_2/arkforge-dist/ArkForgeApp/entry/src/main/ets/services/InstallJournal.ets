import { bundleManager, common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { AtomicFiles } from './AtomicFiles';

interface PendingInstall {
  beforeUpdateTime: number;
  requestedAt: number;
  hapFile: string;
  moduleName: string;
  state: string;
  candidate?: string;
}
interface InstallCandidate { moduleName: string; abcSha256: string; }

/** A self-update can terminate this process before the HDC callback arrives. */
export class InstallJournal {
  private static path(context: common.UIAbilityContext): string {
    return context.filesDir + '/native-toolchain/install-job.json';
  }

  private static save(context: common.UIAbilityContext, job: PendingInstall): void {
    AtomicFiles.write(InstallJournal.path(context), JSON.stringify(job));
  }

  static async begin(context: common.UIAbilityContext, hap: string): Promise<void> {
    const info = await bundleManager.getBundleInfoForSelf(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_HAP_MODULE);
    const candidate = context.filesDir + '/offline-build/pending-install.json';
    let moduleName = 'harmony_demo';
    if (fileIo.accessSync(candidate)) moduleName = (JSON.parse(fileIo.readTextSync(candidate)) as InstallCandidate).moduleName;
    InstallJournal.save(context, { beforeUpdateTime: info.updateTime, requestedAt: Date.now(),
      hapFile: hap, moduleName: moduleName, state: 'submitted', candidate:fileIo.accessSync(candidate) ? candidate : undefined });
  }

  static fail(context: common.UIAbilityContext): void {
    const path = InstallJournal.path(context);
    if (!fileIo.accessSync(path)) return;
    const job = JSON.parse(fileIo.readTextSync(path)) as PendingInstall;
    job.state = 'failed';
    InstallJournal.save(context, job);
  }

  static async recover(context: common.UIAbilityContext): Promise<string> {
    const path = InstallJournal.path(context);
    if (!fileIo.accessSync(path)) return '';
    const job = JSON.parse(fileIo.readTextSync(path)) as PendingInstall;
    if (job.state !== 'submitted') return '';
    const info = await bundleManager.getBundleInfoForSelf(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_HAP_MODULE);
    const present = info.hapModulesInfo.some((item: bundleManager.HapModuleInfo) => item.name === job.moduleName);
    if (info.updateTime <= job.beforeUpdateTime || !present) return '上次本机安装尚未获得系统更新确认。';
    if (job.candidate) {
      if (job.candidate !== context.filesDir + '/offline-build/pending-install.json') throw new Error('安装恢复记录无效');
      const text = fileIo.readTextSync(job.candidate), candidate = JSON.parse(text) as InstallCandidate;
      if (!/^[A-Za-z0-9_]+$/.test(candidate.moduleName)) throw new Error('安装模块标识无效');
      const installedAbc = '/data/storage/el1/bundle/' + candidate.moduleName + '/ets/modules.abc';
      if (!fileIo.accessSync(installedAbc) || await hash.hash(installedAbc,'sha256') !== candidate.abcSha256) return '应用已更新，但尚未确认本次编译字节码，未替换最近成功记录。';
      AtomicFiles.write(context.filesDir + '/offline-build/latest.json', text);
      fileIo.unlinkSync(job.candidate);
    }
    job.state = 'updated';
    InstallJournal.save(context, job);
    return '系统已确认应用更新，已恢复上次安装任务。\n' + job.hapFile +
      '\n可从“调试”菜单启动已安装示例。';
  }
}
