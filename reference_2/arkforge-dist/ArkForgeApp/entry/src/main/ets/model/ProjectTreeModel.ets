export interface ProjectTreeEntry {
  path: string;
  name: string;
  parent: string;
  directory: boolean;
  depth: number;
}

/** Build a hierarchy from workspace-relative paths; no filesystem access or build work. */
export class ProjectTreeModel {
  static entries(files: string[]): ProjectTreeEntry[] {
    const entries = new Map<string, ProjectTreeEntry>();
    for (const file of files) {
      const parts = file.split('/');
      if (parts.some((part: string) => part === '..' || part === '.' || part.length === 0)) continue;
      for (let i = 0; i < parts.length; i++) {
        const path = parts.slice(0, i + 1).join('/');
        entries.set(path, {
          path: path, name: parts[i], parent: parts.slice(0, i).join('/'),
          directory: i < parts.length - 1, depth: i
        });
      }
    }
    return Array.from(entries.values());
  }

  static reveal(path: string, expanded: string[]): string[] {
    const result = expanded.slice();
    const parts = path.split('/');
    for (let i = 1; i < parts.length; i++) {
      const parent = parts.slice(0, i).join('/');
      if (!result.includes(parent)) result.push(parent);
    }
    return result;
  }

  static visible(entries: ProjectTreeEntry[], expanded: string[]): ProjectTreeEntry[] {
    const children = new Map<string, ProjectTreeEntry[]>();
    for (const entry of entries) {
      const siblings = children.get(entry.parent) ?? [];
      siblings.push(entry);
      children.set(entry.parent, siblings);
    }
    const result: ProjectTreeEntry[] = [];
    ProjectTreeModel.append('', children, expanded, result);
    return result;
  }

  private static append(parent: string, children: Map<string, ProjectTreeEntry[]>,
    expanded: string[], result: ProjectTreeEntry[]): void {
    const siblings = (children.get(parent) ?? []).slice();
    siblings.sort((a: ProjectTreeEntry, b: ProjectTreeEntry) => {
      if (a.directory !== b.directory) return a.directory ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
    for (const entry of siblings) {
      result.push(entry);
      if (entry.directory && expanded.includes(entry.path)) {
        ProjectTreeModel.append(entry.path, children, expanded, result);
      }
    }
  }
}
