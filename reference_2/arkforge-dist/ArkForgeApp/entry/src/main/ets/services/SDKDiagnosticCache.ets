import { fileIo, hash } from '@kit.CoreFileKit';
import { AtomicFiles } from './AtomicFiles';
import { SDKResult, SDKDiagnostic } from './SDKProjectChecker';

/** Diagnostics are part of a successful compilation checkpoint, including warnings. */
export class SDKDiagnosticCache {
  static async load(root: string, expectedHash?: string): Promise<SDKResult | undefined> {
    const path = root + '/sdk-check-result.json';
    try {
      if (!expectedHash || !fileIo.accessSync(path) || await hash.hash(path, 'sha256') !== expectedHash) return undefined;
      const result = JSON.parse(fileIo.readTextSync(path)) as SDKResult;
      if (!result.ok || !Array.isArray(result.diagnostics) ||
        result.diagnostics.some((item: SDKDiagnostic) => item.category === 1)) return undefined;
      return result;
    } catch (_) { return undefined; }
  }

  static publish(filesDir: string, project: string, root: string, result: SDKResult): void {
    if (!Array.isArray(result.diagnostics)) throw new Error('请重新执行 SDK 检查以取得完整诊断');
    AtomicFiles.write(root + '/sdk-check-result.json', JSON.stringify(result));
    AtomicFiles.write(filesDir + '/latest-sdk-diagnostics.json', JSON.stringify({ project, status: 'checked', diagnostics: result.diagnostics }));
  }
}
