import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog, hiAppEvent } from '@kit.PerformanceAnalysisKit';
import { fileIo } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';

/**
 * 应用入口。
 * 注意：本 App 不需要任何特殊系统权限，
 * 也不创建子进程——脚本引擎以 .so 形式在本进程内加载。
 */
export default class EntryAbility extends UIAbility {
  onCreate(): void {
    hilog.info(0x0000, 'Playground', 'Ability onCreate');
    hiAppEvent.addWatcher({
      name: 'ArkForgeNativeCrash',
      appEventFilters: [{ domain: 'OS', names: [hiAppEvent.event.APP_CRASH] }],
      onReceive: (domain: string, groups: hiAppEvent.AppEventGroup[]): void => {
        const path = this.context.filesDir + '/native-crash.json';
        const file = fileIo.openSync(path,
          fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
        try { fileIo.writeSync(file.fd, JSON.stringify(groups)); } finally { fileIo.closeSync(file); }
        hilog.error(0x0000, 'ArkForgeNative', 'Native crash event saved in application files');
      }
    });
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.getMainWindowSync().setWindowKeepScreenOn(true).catch((error: BusinessError) => {
      hilog.warn(0x0000, 'ArkForge', 'Keep screen on failed: %{public}d', error.code);
    });
    windowStage.loadContent('pages/Index', (err: BusinessError) => {
      if (err.code) {
        hilog.error(0x0000, 'Playground',
          `Failed to load content. Code=${err.code}, message=${err.message}`);
        return;
      }
      hilog.info(0x0000, 'Playground', 'Succeeded in loading content.');
    });
  }

  onDestroy(): void {
    hilog.info(0x0000, 'Playground', 'Ability onDestroy');
  }
}
