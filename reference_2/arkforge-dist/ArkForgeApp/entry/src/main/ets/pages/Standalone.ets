/**
 * Standalone.ets —— 独立运行页（应用外展示）
 *
 * 本页直接读取应用沙盒里导出的 usercode.js，在本机 QuickJS 中执行，
 * 再用 DynamicRenderer 全屏渲染。相当于把用户在 ArkForge IDE 里写的工程，
 * 以独立图标 / 独立窗口运行。
 *
 * StandaloneAbility 每次被拉起都会重新 loadContent 本页，
 * 因此这里的 onPageShow() 一定会读到「最新一次导出」的代码，
 * 不会出现残留上一次运行结果的情况。
 */
import { DynamicRenderer } from '../view/DynamicRenderer';
import { UiNode } from '../model/UiNode';
import { Jsvm, EvalResult } from '../bridge/Jsvm';
import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { hilog } from '@kit.PerformanceAnalysisKit';

@Entry
@Component
struct Standalone {
  @State private uiTree: UiNode | null = null;
  @State private status: string = '正在读取导出的工程…';
  /** 每次重新加载都自增，强制 DynamicRenderer 整棵子树重新挂载，
   *  避免「数据已更新但界面残留上一次渲染」的竞态（同 Ability 再次拉起时常见）。 */
  @State private renderKey: number = 0;

  /** 页面每次显示都重新加载最新导出的代码 */
  onPageShow(): void {
    this.loadAndRun();
  }

  private loadAndRun(): void {
    const path = getContext(this).filesDir + '/usercode.js';
    try {
      const f = fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
      const stat = fileIo.statSync(path);
      const buf = new ArrayBuffer(stat.size);
      fileIo.readSync(f.fd, buf);
      fileIo.closeSync(f.fd);
      const code: string = util.TextDecoder.create('utf-8').decodeToString(new Uint8Array(buf));

      hilog.info(0x0000, 'ArkForge', 'standalone code len=%{public}d head=%{public}s',
        code.length, code.substring(0, 60));

      // 独立运行每次都是「全新一次启动」，先重置 VM：
      // 否则同一段代码被重复执行时，let/const 声明会报 redeclaration。
      Jsvm.reset();
      const r: EvalResult = Jsvm.evalScript(code);
      hilog.info(0x0000, 'ArkForge', 'standalone page eval ok=%{public}s timeout=%{public}s',
        String(r.ok), String(r.timeout));
      if (!r.ok) {
        hilog.error(0x0000, 'ArkForge', 'standalone page eval error: %{public}s', r.error);
      }

      this.uiTree = r.ui;
      this.renderKey++;
      this.status = r.ok
        ? '已运行导出的工程，但其中没有调用 render()'
        : ('执行出错：' + r.error);
    } catch (e) {
      this.uiTree = null;
      this.status = '未找到导出的工程。\n请先在 ArkForge Playground 里点「导出独立App」。';
      hilog.error(0x0000, 'ArkForge', 'standalone page load failed: %{public}s', JSON.stringify(e));
    }
  }

  build() {
    Column() {
      if (this.uiTree === null) {
        Text(this.status)
          .fontSize(15)
          .fontColor('#6B7280')
          .textAlign(TextAlign.Center)
          .padding(20)
      } else {
        DynamicRenderer({ node: this.uiTree }).key('rk' + this.renderKey)
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#FBFBFC')
    .justifyContent(FlexAlign.Center)
  }
}
