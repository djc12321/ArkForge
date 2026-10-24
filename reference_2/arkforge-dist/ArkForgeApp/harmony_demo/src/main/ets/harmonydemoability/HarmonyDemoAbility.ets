import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

export default class HarmonyDemoAbility extends UIAbility {
  onWindowStageCreate(stage: window.WindowStage): void {
    stage.loadContent('pages/Index').then(() => {
      hilog.info(0, 'ArkForgeHarmonyDemo', 'Native ArkUI page loaded; build=2');
    }).catch((error: Error) => {
      hilog.error(0, 'ArkForgeHarmonyDemo', 'Load failed: %{public}s', error.message);
    });
  }
}
