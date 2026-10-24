import { worker, MessageEvents, util } from '@kit.ArkTS';
import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import native from 'libarkforge_script.so';
import { BuildProgress } from './BuildConsole';

interface CompileResult {
  ok: boolean;
  state: number;
  output: string;
  error: string;
}

/** Static ArkTS C API diagnostic. Its ABC is not the dynamic ArkUI modules.abc. */
export class StaticArkCompiler {
  private static busy: boolean = false;

  private static write(path: string, text: string): void {
    const file = fileIo.openSync(path,
      fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try { fileIo.writeSync(file.fd, text); } finally { fileIo.closeSync(file); }
  }

  private static read(path: string): Uint8Array {
    const bytes = new ArrayBuffer(fileIo.statSync(path).size);
    const file = fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
    try { fileIo.readSync(file.fd, bytes); } finally { fileIo.closeSync(file); }
    return new Uint8Array(bytes);
  }

  private static compile(thread: worker.ThreadWorker, input: string,
    output: string, anchor: string): Promise<CompileResult> {
    return new Promise<CompileResult>((resolve, reject) => {
      thread.onmessage = (event: MessageEvents): void => {
        resolve(JSON.parse(event.data as string) as CompileResult);
      };
      thread.onerror = (): void => { reject(new Error('编译工作线程异常')); };
      thread.postMessage({ input: input, output: output, resourceAnchor: anchor });
    });
  }

  static async probe(context: common.UIAbilityContext, report?: BuildProgress): Promise<string> {
    if (StaticArkCompiler.busy) throw new Error('方舟编译器正在运行');
    StaticArkCompiler.busy = true;
    let thread: worker.ThreadWorker | undefined = undefined;
    try {
      report?.('> ArkForge :loadArm64StaticCompiler');
      const loader = native.inspectLibrary('libes2panda-public.so');
      hilog.info(0x0000, 'ArkForgeNative', 'Compiler loader: %{public}s', loader);
      if (loader !== 'loaded') throw new Error(loader);
      const root = context.filesDir + '/native-toolchain/compiler';
      if (!fileIo.accessSync(root + '/bin')) fileIo.mkdirSync(root + '/bin', true);
      if (!fileIo.accessSync(root + '/decls')) fileIo.mkdirSync(root + '/decls', true);
      report?.('> ArkForge :prepareCompilerResources');
      const stdlib = await context.resourceManager.getRawFileContent('compiler/etsstdlib.abc');
      const file = fileIo.openSync(root + '/etsstdlib.abc',
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      try { fileIo.writeSync(file.fd, stdlib.buffer); } finally { fileIo.closeSync(file); }
      const template = await context.resourceManager.getRawFileContent('compiler/arktsconfig.template.json');
      const config = new util.TextDecoder().decodeToString(template).replace(/__COMPILER_DIR__/g, root);
      StaticArkCompiler.write(root + '/bin/arktsconfig.json', config);
      const anchor = root + '/bin/es2abc';
      StaticArkCompiler.write(anchor, 'C API resource anchor; never executed.');
      thread = new worker.ThreadWorker('entry/ets/workers/CompilerWorker.ets');
      const input = root + '/hello.ets';
      const output1 = root + '/device-v1.abc';
      const output2 = root + '/device-v2.abc';
      StaticArkCompiler.write(input, 'function main(): void { let message: String = "ArkForge device v1"; console.log(message); }');
      report?.('> CAPI compileArkTS ' + input + ' --output ' + output1);
      const first = await StaticArkCompiler.compile(thread, input, output1, anchor);
      if (!first.ok) throw new Error('第一次编译：' + first.error);
      StaticArkCompiler.write(input, 'function main(): void { let message: String = "ArkForge device v2"; console.log(message); }');
      report?.('> CAPI compileArkTS ' + input + ' --output ' + output2);
      const second = await StaticArkCompiler.compile(thread, input, output2, anchor);
      if (!second.ok) throw new Error('第二次编译：' + second.error);
      const bytes1 = StaticArkCompiler.read(output1);
      const bytes2 = StaticArkCompiler.read(output2);
      const header = new util.TextDecoder().decodeToString(bytes2.slice(0, 5));
      let changed = bytes1.length !== bytes2.length;
      for (let i = 0; i < Math.min(bytes1.length, bytes2.length); i++) {
        if (bytes1[i] !== bytes2[i]) changed = true;
      }
      const invalidOutput = root + '/invalid.abc';
      StaticArkCompiler.write(input, 'function main(: void {');
      report?.('> ArkForge :validateInvalidSource (下方编译诊断是预期的拒绝测试)');
      const invalid = await StaticArkCompiler.compile(thread, input, invalidOutput, anchor);
      const rejected = !invalid.ok && !fileIo.accessSync(invalidOutput);
      const ok = first.state === 6 && second.state === 6 && header === 'PANDA' && changed && rejected;
      const resultReport = JSON.stringify({ stage: 'static-arkts-device-compile', ok: ok,
        firstBytes: bytes1.length, secondBytes: bytes2.length, changed: changed, invalidRejected: rejected });
      StaticArkCompiler.write(root + '/device-result.json', resultReport);
      hilog.info(0x0000, 'ArkForgeNative', '%{public}s', resultReport);
      if (!ok) throw new Error(resultReport);
      return '平板进程内两次静态 ArkTS 编译及错误输入检查通过。\n' + resultReport +
        '\n这是静态编译器验证，尚不代表传统 ArkUI 工程已构建为可安装 HAP。';
    } finally {
      if (thread) thread.terminate();
      StaticArkCompiler.busy = false;
    }
  }
}
