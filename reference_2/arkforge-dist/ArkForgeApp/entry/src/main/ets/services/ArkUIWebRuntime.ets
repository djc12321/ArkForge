import { common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { AtomicFiles } from './AtomicFiles';

interface TransformWrapper { source: string; sha256: string; wrapper: number; }

/** Run the same SDK transformer bundle in ArkWeb; its SDK code stays unchanged. */
export class ArkUIWebRuntime {
  static async stage(context: common.UIAbilityContext, bundle: string, sourceSha: string): Promise<string> {
    const directory = context.filesDir + '/arkui-runtime';
    if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory, true);
    const target = directory + '/ui-transform.web.js';
    const metadata = target + '.json';
    try {
      const previous = JSON.parse(fileIo.readTextSync(metadata)) as TransformWrapper;
      if (previous.source === sourceSha && previous.wrapper === 1 && fileIo.accessSync(target) &&
        await hash.hash(target, 'sha256') === previous.sha256) return target;
    } catch (_) { /* Regenerate a missing or changed wrapper. */ }
    const source = fileIo.readTextSync(bundle);
    const marker = '\n(function(){var api=__load("/runtime.cjs")';
    const offset = source.lastIndexOf(marker);
    if (offset < 0 || !source.endsWith('})()')) throw new Error('Unsupported ArkUI bundle wrapper');
    AtomicFiles.write(target, source.substring(0, offset) + '\nglobalThis.__arkforgeTransformResult=' + source.substring(offset + 1) + ';');
    const record: TransformWrapper = {source:sourceSha,sha256:await hash.hash(target,'sha256'),wrapper:1};
    AtomicFiles.write(metadata, JSON.stringify(record));
    return target;
  }
}
