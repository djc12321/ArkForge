import { worker, MessageEvents, ThreadWorkerGlobalScope } from '@kit.ArkTS';
import compiler from 'libes2abc.so';

interface CompileRequest {
  input: string;
  output: string;
  resourceAnchor: string;
}

const port: ThreadWorkerGlobalScope = worker.workerPort;
port.onmessage = (event: MessageEvents): void => {
  const request = event.data as CompileRequest;
  try {
    const result = compiler.compileArkTS(request.input, request.output, request.resourceAnchor);
    port.postMessage(JSON.stringify(result));
  } catch (error) {
    port.postMessage(JSON.stringify({ ok: false, state: -1, output: request.output, error: String(error) }));
  }
};
