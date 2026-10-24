import { worker, MessageEvents, ThreadWorkerGlobalScope } from '@kit.ArkTS';
import { fileIo } from '@kit.CoreFileKit';
import compiler from 'libarkforge_dynamic.so';
import scriptEngine from 'libarkforge_script.so';
import signer from 'libsigntool.so';
import resources from 'libarkforge_resources.so';

interface Request {
  kind: string;
  bundlePath: string;
  source: string;
  fileName: string;
  args: string[];
}
const port: ThreadWorkerGlobalScope = worker.workerPort;
let projectOptions: string = '{}';
port.onmessage = (event: MessageEvents): void => {
  const request = event.data as Request;
  try {
    if (request.kind === 'resource') {
      const status = resources.compile(request.args);
      port.postMessage(JSON.stringify({ ok: status === 0, status: status }));
    } else if (request.kind === 'configure') {
      projectOptions = request.source;
      port.postMessage(JSON.stringify({ ok: true }));
    } else if (request.kind === 'transformBatch') {
      const script = 'var __arkforgeBatchFileNames=' + JSON.stringify(request.args) +
        ';var __arkforgeOptions=' + projectOptions + ';\n' + fileIo.readTextSync(request.bundlePath);
      port.postMessage(scriptEngine.evalCompiler(script));
    } else if (request.kind === 'transform') {
      const script = 'var __arkforgeSource=' + JSON.stringify(request.source) +
        ';var __arkforgeFileName=' + JSON.stringify(request.fileName) +
        ';var __arkforgeOptions=' + projectOptions + ';\n' + fileIo.readTextSync(request.bundlePath);
      port.postMessage(scriptEngine.evalCompiler(script));
    } else if (request.kind === 'pack') {
      signer.packHap(request.args[0], request.args[1], request.args[2]);
      port.postMessage(JSON.stringify({ ok: true }));
    } else {
      const status = compiler.compile(request.args);
      port.postMessage(JSON.stringify({ ok: status === 0, status: status }));
    }
  } catch (error) {
    port.postMessage(JSON.stringify({ ok: false, error: String(error) }));
  }
};
