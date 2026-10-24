import { fileIo } from '@kit.CoreFileKit';

export interface SourcePosition { path: string; line: number; column: number; }
export interface SourceMapEntry { path: string; javascript: string; mapFile: string; }
interface StandardMap { version: number; sources: string[]; mappings: string; }
export interface SourceMapFile { sourceMap: string; generatedLineMap: number[]; }
interface Mapping { generatedLine: number; generatedColumn: number; line: number; column: number; }

/** Standard source-map VLQ decoding; all public locations are one-based. */
export class SourceMaps {
  static path(url: string): string {
    const fields = url.split('|');
    return fields.length >= 4 ? fields[0] + '/' + fields[3] : url;
  }
  private entries: SourceMapEntry[] = [];
  constructor(root: string) {
    if (fileIo.accessSync(root + '/source-maps.json')) this.entries = JSON.parse(fileIo.readTextSync(root + '/source-maps.json')) as SourceMapEntry[];
  }
  private decode(entry: SourceMapEntry): Mapping[] {
    const data = JSON.parse(fileIo.readTextSync(entry.mapFile)) as SourceMapFile;
    const map = JSON.parse(data.sourceMap) as StandardMap;
    if (map.version !== 3) throw new Error('不支持的 source map 版本');
    const positions: Mapping[][] = [];
    let source = 0, line = 0, column = 0;
    const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    for (const row of map.mappings.split(';')) {
      const items: Mapping[] = []; let generatedColumn = 0;
      for (const segment of row.split(',')) {
        if (!segment) continue;
        const values: number[] = []; let number = 0, shift = 0;
        for (const character of segment) {
          const digit = alphabet.indexOf(character);
          if (digit < 0) throw new Error('source map 编码无效');
          number += (digit & 31) * Math.pow(2, shift);
          if (digit & 32) shift += 5;
          else { values.push((number & 1) ? -Math.floor(number / 2) : Math.floor(number / 2)); number = 0; shift = 0; }
        }
        generatedColumn += values[0];
        if (values.length >= 4) {
          source += values[1]; line += values[2]; column += values[3];
          if (source === 0) items.push({generatedLine:positions.length,generatedColumn:generatedColumn,line:line + 1,column:column + 1});
        }
      }
      positions.push(items);
    }
    const result: Mapping[] = [];
    data.generatedLineMap.forEach((original: number, generated: number) => {
      if (original >= 0 && positions[original]) for (const item of positions[original]) result.push({generatedLine:generated,generatedColumn:item.generatedColumn,line:item.line,column:item.column});
    });
    return result;
  }
  private find(url: string): SourceMapEntry | undefined {
    url = SourceMaps.path(url);
    return this.entries.find((entry: SourceMapEntry) => url === entry.path || url === entry.javascript || url.endsWith('/' + entry.path) ||
      url.endsWith('/' + entry.path.replace(/\.(ets|ts)$/, '.js')) || url.includes(entry.path.replace(/\.(ets|ts)$/, '') + '&'));
  }
  original(url: string, generatedLine: number, generatedColumn: number): SourcePosition | undefined {
    const entry = this.find(url); if (!entry) return undefined;
    const values = this.decode(entry).filter((item: Mapping) => item.generatedLine === generatedLine && item.generatedColumn <= generatedColumn);
    const item = values[values.length - 1];
    return item ? {path:entry.path,line:item.line,column:item.column} : undefined;
  }
  generated(path: string, line: number): number | undefined {
    const entry = this.entries.find((item: SourceMapEntry) => item.path === path); if (!entry) return undefined;
    const matches = this.decode(entry).filter((item: Mapping) => item.line === line);
    return matches.length ? matches[0].generatedLine : undefined;
  }
}
