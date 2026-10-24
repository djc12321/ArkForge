/**
 * Jsvm.ets —— NAPI 桥接封装（ArkTS 侧）
 *
 * 调用的 libarkforge_script.so 就是 native/jsvm_bridge.c 编译产物。
 * 关键点：这里没有任何 fork/exec，全部是同进程内的函数调用，
 * 因此不触碰 HarmonyOS 对普通应用的进程创建限制。
 */
import native from 'libarkforge_script.so';
import { UiNode } from '../model/UiNode';

/** Native 层返回的原始 JSON 结构 */
export interface EvalResult {
  ok: boolean;
  value: string;      // 最后表达式的值（JSON 字符串形式）
  logs: string;       // console.log 累积输出（已拼接换行）
  ui: UiNode | null;  // 最后一次 render() 传入的 UI 树
  error: string;
  timeout: boolean;   // 是否因超时被中止（防死循环）
  timeMs: number;
}

export class Jsvm {
  /**
   * 执行一段 JS 代码。
   * 主线程调用即可——Native 层已内置 2 秒中断保护，不会拖死 UI。
   */
  static evalScript(code: string): EvalResult {
    const raw: string = native.evalScript(code);
    try {
      const parsed = JSON.parse(raw) as EvalResult;
      return parsed;
    } catch (e) {
      return {
        ok: false,
        value: '',
        logs: '',
        ui: null,
        error: `结果解析失败：${raw}`,
        timeout: false,
        timeMs: 0
      };
    }
  }

  /** 重置 VM：清空全局变量状态，从头开始 */
  static reset(): void {
    native.resetVM();
  }
}

/** 默认示例代码 */
export const SAMPLE_CODE: string =
`// 1) 控制台输出
console.log('Hello, MatePad!');

// 2) 计算
let sum = 0;
for (let i = 1; i <= 100; i++) sum += i;
console.log('1+2+...+100 =', sum);

// 3) 描述一个 UI 并渲染到上方预览区
render(
  ui.column([
    ui.text('本机解释器已就绪', { size: 22, weight: 'bold', color: '#0F7B4F' }),
    ui.text('1..100 求和 = ' + sum, { size: 16 }),
    ui.row([
      ui.text('左'),
      ui.text('右')
    ], { space: 12 })
  ], { align: 'center', space: 10 })
);

// 4) 返回最后一个表达式的值
sum;`;
