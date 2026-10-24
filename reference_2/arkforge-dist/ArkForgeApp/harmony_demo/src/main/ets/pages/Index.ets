import { hilog } from '@kit.PerformanceAnalysisKit';
import { common } from '@kit.AbilityKit';

@Entry
@Component
struct Index {
  @State private count: number = 0;

  build() {
    Column({ space: 24 }) {
      Text('真实 HarmonyOS 工程').fontSize(30).fontWeight(FontWeight.Bold).fontColor('#111827')
      Text('ArkTS → ArkUI → ets/modules.abc → harmony_demo.hap')
        .fontSize(17).fontColor('#475569')
      Text('构建版本 2').fontSize(18).fontColor('#2464A4')
      Text('点击次数：' + this.count).fontSize(24).fontColor('#111827')
      Button('验证原生 ArkUI 交互').height(48).backgroundColor('#2464A4')
        .onClick(() => {
          this.count++;
          hilog.info(0, 'ArkForgeHarmonyDemo', 'Native counter=%{public}d build=2', this.count);
        })
      Text('源码：harmony_demo/src/main/ets/pages/Index.ets')
        .fontSize(14).fontColor('#64748B')
      Button('返回 ArkForge 工程').height(48).backgroundColor('#475569')
        .onClick(() => {
          const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
          context.startAbility({ bundleName: 'com.djc.arkforge', abilityName: 'EntryAbility' })
            .catch((error: Error) => {
              hilog.error(0, 'ArkForgeHarmonyDemo', 'Return failed: %{public}s', error.message);
            });
        })
    }.width('100%').height('100%').justifyContent(FlexAlign.Center)
    .padding(24).backgroundColor('#F4F7FB')
  }
}
