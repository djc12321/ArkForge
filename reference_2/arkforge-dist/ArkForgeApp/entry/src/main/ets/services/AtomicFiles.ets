import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';

/** Readers see either the complete previous file or the complete new file. */
export class AtomicFiles {
  private static sequence: number = 0;
  static write(path: string, text: string): void {
    AtomicFiles.writeBytes(path, new util.TextEncoder().encodeInto(text));
  }
  static writeBytes(path: string, bytes: Uint8Array): void {
    const temporary = path + '.arkforge-tmp-' + Date.now() + '-' + (++AtomicFiles.sequence);
    const file = fileIo.openSync(temporary, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try {
      let offset = 0;
      while (offset < bytes.length) {
        const written = fileIo.writeSync(file.fd, bytes.slice(offset).buffer);
        if (written <= 0) throw new Error('文件写入未完成：' + path);
        offset += written;
      }
      fileIo.fsyncSync(file.fd);
    } catch (error) {
      fileIo.closeSync(file);
      if (fileIo.accessSync(temporary)) fileIo.unlinkSync(temporary);
      throw new Error(String(error));
    }
    fileIo.closeSync(file);
    try { fileIo.renameSync(temporary, path); }
    catch (error) { if (fileIo.accessSync(temporary)) fileIo.unlinkSync(temporary); throw new Error(String(error)); }
  }
}
