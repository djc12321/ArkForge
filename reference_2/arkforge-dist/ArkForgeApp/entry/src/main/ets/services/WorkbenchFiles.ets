import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { AtomicFiles } from './AtomicFiles';
import { WorkspaceRecovery } from './WorkspaceRecovery';

interface FileRequest { path: string; target?: string; data?: string; overwrite?: boolean; create?: boolean; files?: string[]; maxFileSize?: number; pattern?: string; matchCase?: boolean; }
interface SearchContent { name: string; text: string; }

/** VS Code's filesystem provider resolves exclusively inside the opened project. */
export class WorkbenchFiles {
  private static index(root: string, relative: string, files: string[], depth: number): void {
    if (depth > 24) return;
    for (const name of fileIo.listFileSync(root + (relative ? '/' + relative : ''))) {
      if (['.git','.arkforge','.hvigor','.cxx','build','node_modules','oh_modules'].includes(name)) continue;
      const next = relative ? relative + '/' + name : name;
      const stat = fileIo.lstatSync(root + '/' + next);
      if (stat.isSymbolicLink()) continue;
      if (stat.isDirectory()) WorkbenchFiles.index(root,next,files,depth + 1);
      else files.push(next);
      if (files.length > 20000) throw new Error('搜索文件超过 20000 个，请缩小搜索目录');
    }
  }
  private static path(root: string, uriPath: string, allowRoot: boolean = true): string {
    if (uriPath !== '/workspace' && !uriPath.startsWith('/workspace/')) throw new Error('无效的工程 URI');
    const relative = uriPath.slice('/workspace'.length).replace(/^\//, '');
    const parts = relative ? relative.split('/') : [];
    if ((!allowRoot && !parts.length) || parts.some((part: string) => !part || part === '.' || part === '..' || part.includes('\\') || part.includes('\u0000'))) throw new Error('无效的工程路径');
    let path = root;
    for (const part of parts) {
      path += '/' + part;
      if (fileIo.accessSync(path) && fileIo.lstatSync(path).isSymbolicLink()) throw new Error('不能通过符号链接访问工程外文件');
    }
    return path;
  }

  static execute(root: string, operation: string, payload: string): string {
    const input = JSON.parse(payload) as FileRequest;
    const path = WorkbenchFiles.path(root, input.path, !['delete','rename','write'].includes(operation));
    if (operation === 'readBatch') {
      const items: SearchContent[] = []; let consumed = 0; let total = 0; let unreadable = 0;
      const matcher = input.pattern ? new RegExp(input.pattern,input.matchCase ? 'mu' : 'imu') : undefined;
      for (const name of (input.files || []).slice(0,128)) {
        const file = WorkbenchFiles.path(root,input.path + '/' + name,false);
        try {
          const stat = fileIo.statSync(file);
          if (!stat.isFile() || stat.size > Math.min(input.maxFileSize || 2097152,2097152)) { consumed++; continue; }
          const text = fileIo.readTextSync(file);
          if (!text.slice(0,1024).includes('\u0000') && (!matcher || matcher.test(text))) {
            if (total + stat.size > 4194304) break;
            items.push({name:name,text:text}); total += stat.size;
          }
        } catch (_) { unreadable++; }
        consumed++;
      }
      return JSON.stringify({items:items,consumed:consumed,unreadable:unreadable});
    }
    if (operation === 'index') {
      const files: string[] = [];
      WorkbenchFiles.index(path,'',files,0);
      return JSON.stringify(files);
    }
    if (operation === 'stat') {
      const stat = fileIo.statSync(path);
      return JSON.stringify({type:stat.isDirectory() ? 2 : 1, size:stat.size, ctime:stat.ctime * 1000, mtime:stat.mtime * 1000});
    }
    if (operation === 'list') {
      return JSON.stringify(fileIo.listFileSync(path).map((name: string) => {
        const item = fileIo.lstatSync(path + '/' + name);
        return [name, item.isSymbolicLink() ? 64 : item.isDirectory() ? 2 : 1];
      }));
    }
    if (operation === 'read') {
      const stat = fileIo.statSync(path);
      if (stat.size === 0) return '';
      if (stat.size > 32 * 1024 * 1024) throw new Error('文件超过 32 MB，不能在编辑器中打开');
      const file = fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
      const data = new Uint8Array(stat.size);
      try {
        let offset = 0;
        while (offset < data.length) {
          const chunk = new ArrayBuffer(data.length - offset);
          const count = fileIo.readSync(file.fd, chunk);
          if (!count) break;
          data.set(new Uint8Array(chunk,0,count),offset); offset += count;
        }
        return new util.Base64Helper().encodeToStringSync(data);
      } finally { fileIo.closeSync(file); }
    }
    if (operation === 'write') {
      if (fileIo.accessSync(path) && input.overwrite === false) throw new Error('目标文件已存在');
      if (!fileIo.accessSync(path) && input.create === false) throw new Error('文件不存在');
      const data = input.data ? new util.Base64Helper().decodeSync(input.data) : new Uint8Array(0);
      if (data.length > 32 * 1024 * 1024) throw new Error('文件超过 32 MB');
      const relative = path.slice(root.length + 1);
      if (!relative.split('/').includes('.git')) WorkspaceRecovery.checkpoint(root, relative);
      AtomicFiles.writeBytes(path,data);
      return '{}';
    }
    if (operation === 'mkdir') { if (!fileIo.accessSync(path)) fileIo.mkdirSync(path,true); return '{}'; }
    if (operation === 'rename') {
      const destination = WorkbenchFiles.path(root,input.target || '',false);
      if (fileIo.accessSync(destination) && !input.overwrite) throw new Error('目标文件已存在');
      if (fileIo.statSync(path).isFile()) WorkspaceRecovery.checkpoint(root,path.slice(root.length + 1));
      fileIo.renameSync(path,destination); return '{}';
    }
    if (operation === 'delete') {
      if (fileIo.statSync(path).isDirectory()) fileIo.rmdirSync(path);
      else { WorkspaceRecovery.checkpoint(root,path.slice(root.length + 1)); fileIo.unlinkSync(path); }
      return '{}';
    }
    throw new Error('不支持的文件操作');
  }
}
