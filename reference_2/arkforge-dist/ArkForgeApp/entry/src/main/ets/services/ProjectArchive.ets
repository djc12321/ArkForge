import { fileIo } from '@kit.CoreFileKit';
import { zlib } from '@kit.BasicServicesKit';
import resources from 'libarkforge_resources.so';

export interface ImportedProject {
  root: string;
  files: string[];
}

/** Import a ZIP into a separate workspace. Existing projects are never overwritten. */
export class ProjectArchive {
  static async open(uri: string, filesDir: string): Promise<ImportedProject> {
    const root = filesDir + '/imports/' + Date.now();
    fileIo.mkdirSync(root, true);
    const archive = root + '/project.zip';
    const source = fileIo.openSync(uri, fileIo.OpenMode.READ_ONLY);
    try {
      if (fileIo.statSync(source.fd).size > 256 * 1024 * 1024) {
        throw new Error('项目 ZIP 超过 256 MB，请排除 build、oh_modules 和 .git 后重试');
      }
      fileIo.copyFileSync(source.fd, archive);
    } finally {
      fileIo.closeSync(source);
    }
    const extracted = root + '/source';
    fileIo.mkdirSync(extracted);
    resources.validateArchive(archive);
    await zlib.decompressFile(archive, extracted);
    const files: string[] = [];
    ProjectArchive.collect(extracted, '', files, 0);
    if (files.length === 0) throw new Error('ZIP 中没有可编辑源码或工程配置');
    files.sort();
    const manifest = files.find((name: string) => name === 'AppScope/app.json5' || name.endsWith('/AppScope/app.json5'));
    if (!manifest) throw new Error('需要真实 HarmonyOS 工程：ZIP 中缺少 AppScope/app.json5');
    const prefix = manifest.substring(0, manifest.length - 'AppScope/app.json5'.length);
    const projectRoot = extracted + (prefix.length > 0 ? '/' + prefix.substring(0, prefix.length - 1) : '');
    if (!fileIo.accessSync(projectRoot + '/build-profile.json5')) {
      throw new Error('缺少工程根目录 build-profile.json5');
    }
    const projectFiles = files.filter((name: string) => name.startsWith(prefix))
      .map((name: string) => name.substring(prefix.length));
    if (!projectFiles.some((name: string) => name.endsWith('/src/main/module.json5'))) {
      throw new Error('缺少 HarmonyOS 模块清单 src/main/module.json5');
    }
    return { root: projectRoot, files: projectFiles };
  }

  static sourceFiles(root: string): string[] {
    const files: string[] = [];
    ProjectArchive.collect(root,'',files,0);
    return files;
  }
  private static collect(root: string, relative: string, result: string[], depth: number): void {
    if (depth > 20) throw new Error('项目目录超过 20 层');
    const directory = root + (relative.length > 0 ? '/' + relative : '');
    for (const name of fileIo.listFileSync(directory)) {
      if (name === '.git' || name === 'oh_modules' || name === 'node_modules' ||
        name === 'build' || name === '.hvigor' || name === '.cxx') continue;
      const path = relative.length > 0 ? relative + '/' + name : name;
      const stat = fileIo.lstatSync(root + '/' + path);
      if (stat.isSymbolicLink()) continue;
      if (stat.isDirectory()) {
        ProjectArchive.collect(root, path, result, depth + 1);
      } else if (/\.(ets|ts|js|mjs|json5?|md|txt|c|cpp|h|css|html)$/.test(name) || name === 'CMakeLists.txt') {
        if (stat.size > 4 * 1024 * 1024) continue;
        result.push(path);
        if (result.length > 20000) throw new Error('源码文件超过 20000 个，请精简项目 ZIP');
      }
    }
  }
}
