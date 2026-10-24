import { fileIo } from '@kit.CoreFileKit';
import { AtomicFiles } from './AtomicFiles';

export interface EditorDraft { path: string; source: string; savedAt: number; }
export class WorkspaceRecovery {
  static draft(root: string, path: string, source: string): void {
    const directory = root + '/.arkforge';
    if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory, true);
    const draft: EditorDraft = { path: path, source: source, savedAt: Date.now() };
    AtomicFiles.write(directory + '/editor-draft.json', JSON.stringify(draft));
  }
  static read(root: string): EditorDraft | undefined {
    const path = root + '/.arkforge/editor-draft.json';
    if (!fileIo.accessSync(path)) return undefined;
    const draft = JSON.parse(fileIo.readTextSync(path)) as EditorDraft;
    if (!draft.path || draft.path.startsWith('/') || draft.path.includes('..') || typeof draft.source !== 'string') return undefined;
    return draft;
  }
  static clear(root: string, path: string): void {
    const draft = WorkspaceRecovery.read(root);
    if (draft && draft.path === path) fileIo.unlinkSync(root + '/.arkforge/editor-draft.json');
  }
  static checkpoint(root: string, path: string): void {
    if (!fileIo.accessSync(root + '/' + path)) return;
    const history = root + '/.arkforge/history';
    if (!fileIo.accessSync(history)) fileIo.mkdirSync(history, true);
    const name = encodeURIComponent(path) + '.previous';
    fileIo.copyFileSync(root + '/' + path, history + '/' + name);
  }
  static previous(root: string, path: string): string {
    const previous = root + '/.arkforge/history/' + encodeURIComponent(path) + '.previous';
    if (!fileIo.accessSync(previous)) throw new Error('此文件尚无保存历史');
    return fileIo.readTextSync(previous);
  }
}
