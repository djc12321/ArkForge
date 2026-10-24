import { UIAbility, Want, AbilityConstant } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BusinessError } from '@kit.BasicServicesKit';

/**
 * StandaloneAbility —— 独立运行入口（应用外展示）
 *
 * 本 Ability 拥有独立桌面图标（「ArkForge 运行」），
 * 等价于「把用户工程装成独立 App 运行」。
 *
 * 关键设计：每次被拉起（onNewWant / onForeground）都重新 loadContent 一次，
 * 从而彻底重建 pages/Standalone 页面。
 * 这样页面一定是用「最新一次导出的 usercode.js」重新执行并渲染，
 * 不会残留下一次运行的结果。
 */
export default class StandaloneAbility extends UIAbility {
  private stage: window.WindowStage | null = null;

  onCreate(): void {
    hilog.info(0x0000, 'ArkForge', 'StandaloneAbility onCreate');
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(0x0000, 'ArkForge', 'StandaloneAbility onNewWant -> recreate page');
    this.reloadPage();
  }

  onForeground(): void {
    hilog.info(0x0000, 'ArkForge', 'StandaloneAbility onForeground -> recreate page');
    this.reloadPage();
  }

  /** 重建页面：保证每次进入都读到最新的导出结果 */
  private reloadPage(): void {
    this.loadPage();
  }

  private loadPage(): void {
    if (!this.stage) {
      return;
    }
    this.stage.loadContent('pages/Standalone', (err: BusinessError) => {
      if (err.code) {
        hilog.error(0x0000, 'ArkForge',
          `Standalone loadContent failed. Code=${err.code}, message=${err.message}`);
      }
    });
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    this.stage = windowStage;
    this.loadPage();
  }

  onWindowStageDestroy(): void {
    this.stage = null;
  }
}
