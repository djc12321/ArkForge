/**
 * 语法高亮服务
 * 提供代码的语法分析和高亮标记
 *
 * 来源：KeloYuan/xxCode（MIT License）https://github.com/KeloYuan/xxCode
 */

export interface Token {
  type: string;
  value: string;
  start: number;
  end: number;
}

export class SyntaxHighlightService {
  private static instance: SyntaxHighlightService | null = null;

  private constructor() {}

  public static getInstance(): SyntaxHighlightService {
    if (!SyntaxHighlightService.instance) {
      SyntaxHighlightService.instance = new SyntaxHighlightService();
    }
    return SyntaxHighlightService.instance;
  }

  /**
   * 对代码进行语法高亮分析
   */
  public tokenize(code: string, language: string): Token[] {
    switch (language.toLowerCase()) {
      case 'javascript':
      case 'typescript':
      case 'arkts':
      case 'ets':
        return this.tokenizeJavaScript(code);
      case 'python':
        return this.tokenizePython(code);
      case 'java':
        return this.tokenizeJava(code);
      case 'markdown':
        return this.tokenizeMarkdown(code);
      case 'json':
        return this.tokenizeJSON(code);
      case 'html':
        return this.tokenizeHTML(code);
      case 'css':
        return this.tokenizeCSS(code);
      default:
        return this.tokenizeText(code);
    }
  }

  /**
   * JavaScript/TypeScript 语法分析
   */
  private tokenizeJavaScript(code: string): Token[] {
    const tokens: Token[] = [];
    const keywords = ['function', 'const', 'let', 'var', 'if', 'else', 'for', 'while', 
                     'return', 'import', 'export', 'class', 'extends', 'async', 'await',
                     'try', 'catch', 'throw', 'new', 'this', 'super', 'static', 'private',
                     'public', 'protected', 'interface', 'type', 'enum', 'namespace',
                     'struct', 'readonly', 'implements', 'abstract'];
    
    const operators = ['+', '-', '*', '/', '=', '==', '===', '!=', '!==', '<', '>', 
                      '<=', '>=', '&&', '||', '!', '?', ':', '=>'];
    
    let i = 0;
    while (i < code.length) {
      // 保留空白字符
      if (/\s/.test(code[i])) {
        const start = i;
        while (i < code.length && /\s/.test(code[i])) {
          i++;
        }
        tokens.push({ type: 'text', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 单行注释
      if (code.substr(i, 2) === '//') {
        const start = i;
        while (i < code.length && code[i] !== '\n') {
          i++;
        }
        tokens.push({ type: 'comment', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 多行注释
      if (code.substr(i, 2) === '/*') {
        const start = i;
        i += 2;
        while (i < code.length - 1 && code.substr(i, 2) !== '*/') {
          i++;
        }
        i += 2;
        tokens.push({ type: 'comment', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 字符串（双引号）
      if (code[i] === '"') {
        const start = i;
        i++;
        while (i < code.length && code[i] !== '"') {
          if (code[i] === '\\') i++;
          i++;
        }
        i++;
        tokens.push({ type: 'string', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 字符串（单引号）
      if (code[i] === "'") {
        const start = i;
        i++;
        while (i < code.length && code[i] !== "'") {
          if (code[i] === '\\') i++;
          i++;
        }
        i++;
        tokens.push({ type: 'string', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 模板字符串
      if (code[i] === '`') {
        const start = i;
        i++;
        while (i < code.length && code[i] !== '`') {
          if (code[i] === '\\') i++;
          i++;
        }
        i++;
        tokens.push({ type: 'string', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 数字
      if (/\d/.test(code[i])) {
        const start = i;
        while (i < code.length && /[\d.]/.test(code[i])) {
          i++;
        }
        tokens.push({ type: 'number', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 标识符和关键字
      if (/[a-zA-Z_$]/.test(code[i])) {
        const start = i;
        while (i < code.length && /[a-zA-Z0-9_$]/.test(code[i])) {
          i++;
        }
        const value = code.substring(start, i);
        const type = keywords.includes(value) ? 'keyword' : 
                    /^[A-Z]/.test(value) ? 'type' : 'variable';
        tokens.push({ type, value, start, end: i });
        continue;
      }

      // 运算符和括号
      const char = code[i];
      if (operators.includes(char) || '(){}[];,'.includes(char)) {
        const type = operators.includes(char) ? 'operator' : 'bracket';
        tokens.push({ type, value: char, start: i, end: i + 1 });
        i++;
        continue;
      }

      // 其他字符
      tokens.push({ type: 'text', value: code[i], start: i, end: i + 1 });
      i++;
    }

    return tokens;
  }

  /**
   * Python 语法分析
   */
  private tokenizePython(code: string): Token[] {
    const tokens: Token[] = [];
    const keywords = ['def', 'class', 'if', 'elif', 'else', 'for', 'while', 'return',
                     'import', 'from', 'as', 'try', 'except', 'finally', 'with', 'lambda',
                     'pass', 'break', 'continue', 'yield', 'async', 'await', 'True', 'False', 'None'];
    
    // 简化的 Python 分析（类似 JS）
    return this.tokenizeJavaScript(code); // 暂时复用 JS 分析器
  }

  /**
   * Java 语法分析
   */
  private tokenizeJava(code: string): Token[] {
    // 简化实现，复用 JS 分析器
    return this.tokenizeJavaScript(code);
  }

  /**
   * Markdown 语法分析
   */
  private tokenizeMarkdown(code: string): Token[] {
    const tokens: Token[] = [];
    const lines = code.split('\n');
    let pos = 0;

    for (const line of lines) {
      const start = pos;
      
      // 标题
      if (line.startsWith('#')) {
        tokens.push({ type: 'keyword', value: line, start, end: pos + line.length });
      }
      // 代码块
      else if (line.startsWith('```')) {
        tokens.push({ type: 'string', value: line, start, end: pos + line.length });
      }
      // 列表
      else if (/^[-*+]\s/.test(line)) {
        tokens.push({ type: 'keyword', value: line, start, end: pos + line.length });
      }
      // 普通文本
      else {
        tokens.push({ type: 'text', value: line, start, end: pos + line.length });
      }
      
      pos += line.length + 1; // +1 for \n
    }

    return tokens;
  }

  /**
   * JSON 语法分析
   */
  private tokenizeJSON(code: string): Token[] {
    const tokens: Token[] = [];
    let i = 0;

    while (i < code.length) {
      // 保留空白字符
      if (/\s/.test(code[i])) {
        const start = i;
        while (i < code.length && /\s/.test(code[i])) {
          i++;
        }
        tokens.push({ type: 'text', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 字符串（JSON 的键和值）
      if (code[i] === '"') {
        const start = i;
        i++;
        while (i < code.length && code[i] !== '"') {
          if (code[i] === '\\') i++;
          i++;
        }
        i++;
        tokens.push({ type: 'string', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 数字
      if (/[\d-]/.test(code[i])) {
        const start = i;
        while (i < code.length && /[\d.eE+-]/.test(code[i])) {
          i++;
        }
        tokens.push({ type: 'number', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 布尔值和 null
      if (code.substr(i, 4) === 'true' || code.substr(i, 5) === 'false' || code.substr(i, 4) === 'null') {
        const start = i;
        const len = code.substr(i, 5) === 'false' ? 5 : 4;
        i += len;
        tokens.push({ type: 'keyword', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 括号和符号
      if ('{}[]:,'.includes(code[i])) {
        tokens.push({ type: 'bracket', value: code[i], start: i, end: i + 1 });
        i++;
        continue;
      }

      i++;
    }

    return tokens;
  }

  /**
   * HTML 语法分析 - 简化版，保留所有空格
   */
  private tokenizeHTML(code: string): Token[] {
    const tokens: Token[] = [];
    let i = 0;

    while (i < code.length) {
      // 保留空白字符
      if (/\s/.test(code[i])) {
        const start = i;
        while (i < code.length && /\s/.test(code[i])) {
          i++;
        }
        tokens.push({ type: 'text', value: code.substring(start, i), start, end: i });
        continue;
      }

      // HTML 注释 <!-- -->
      if (code.substr(i, 4) === '<!--') {
        const start = i;
        i += 4;
        while (i < code.length - 2 && code.substr(i, 3) !== '-->') {
          i++;
        }
        i += 3;
        tokens.push({ type: 'comment', value: code.substring(start, i), start, end: i });
        continue;
      }

      // DOCTYPE
      if (code.substr(i, 9).toLowerCase() === '<!doctype') {
        const start = i;
        while (i < code.length && code[i] !== '>') {
          i++;
        }
        i++;
        tokens.push({ type: 'keyword', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 标签 < 开始
      if (code[i] === '<') {
        tokens.push({ type: 'bracket', value: '<', start: i, end: i + 1 });
        i++;
        
        // 闭合标签 /
        if (code[i] === '/') {
          tokens.push({ type: 'bracket', value: '/', start: i, end: i + 1 });
          i++;
        }

        // 标签名
        const tagStart = i;
        while (i < code.length && /[a-zA-Z0-9-]/.test(code[i])) {
          i++;
        }
        if (i > tagStart) {
          tokens.push({ type: 'keyword', value: code.substring(tagStart, i), start: tagStart, end: i });
        }
        continue;
      }

      // 标签结束 > 或 />
      if (code[i] === '>') {
        tokens.push({ type: 'bracket', value: '>', start: i, end: i + 1 });
        i++;
        continue;
      }
      if (code[i] === '/' && code[i + 1] === '>') {
        tokens.push({ type: 'bracket', value: '/>', start: i, end: i + 2 });
        i += 2;
        continue;
      }

      // 等号
      if (code[i] === '=') {
        tokens.push({ type: 'operator', value: '=', start: i, end: i + 1 });
        i++;
        continue;
      }

      // 字符串（属性值）
      if (code[i] === '"' || code[i] === "'") {
        const quote = code[i];
        const start = i;
        i++;
        while (i < code.length && code[i] !== quote) {
          i++;
        }
        i++;
        tokens.push({ type: 'string', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 属性名或其他标识符
      if (/[a-zA-Z_\-]/.test(code[i])) {
        const start = i;
        while (i < code.length && /[a-zA-Z0-9_:\-]/.test(code[i])) {
          i++;
        }
        tokens.push({ type: 'variable', value: code.substring(start, i), start, end: i });
        continue;
      }

      // 其他字符
      tokens.push({ type: 'text', value: code[i], start: i, end: i + 1 });
      i++;
    }

    return tokens;
  }

  /**
   * CSS 语法分析
   */
  private tokenizeCSS(code: string): Token[] {
    const tokens: Token[] = [];
    // 简化实现
    tokens.push({ type: 'text', value: code, start: 0, end: code.length });
    return tokens;
  }

  /**
   * 纯文本（无语法高亮，但保留空格处理）
   */
  private tokenizeText(code: string): Token[] {
    const tokens: Token[] = [];
    let i = 0;
    
    while (i < code.length) {
      // 空白字符单独处理
      if (/\s/.test(code[i])) {
        const start = i;
        while (i < code.length && /\s/.test(code[i])) {
          i++;
        }
        tokens.push({ type: 'text', value: code.substring(start, i), start, end: i });
        continue;
      }
      
      // 非空白字符
      const start = i;
      while (i < code.length && !/\s/.test(code[i])) {
        i++;
      }
      tokens.push({ type: 'text', value: code.substring(start, i), start, end: i });
    }
    
    // 如果代码为空，返回空 token
    if (tokens.length === 0) {
      tokens.push({ type: 'text', value: '', start: 0, end: 0 });
    }
    
    return tokens;
  }

  /**
   * 获取语言的显示名称
   */
  public getLanguageDisplayName(language: string): string {
    const nameMap: Record<string, string> = {
      'javascript': 'JavaScript',
      'typescript': 'TypeScript',
      'ets': 'ArkTS',
      'python': 'Python',
      'java': 'Java',
      'cpp': 'C++',
      'c': 'C',
      'go': 'Go',
      'rust': 'Rust',
      'markdown': 'Markdown',
      'html': 'HTML',
      'css': 'CSS',
      'json': 'JSON',
      'xml': 'XML',
      'yaml': 'YAML',
      'text': 'Plain Text'
    };
    return nameMap[language.toLowerCase()] || language;
  }

  /**
   * 根据文件扩展名获取语言类型
   */
  public getLanguageFromExtension(extension: string): string {
    const extMap: Record<string, string> = {
      'js': 'javascript',
      'mjs': 'javascript',
      'cjs': 'javascript',
      'ts': 'typescript',
      'tsx': 'typescript',
      'jsx': 'javascript',
      'ets': 'arkts',
      'py': 'python',
      'java': 'java',
      'c': 'c',
      'cpp': 'cpp',
      'cc': 'cpp',
      'cxx': 'cpp',
      'h': 'c',
      'hpp': 'cpp',
      'go': 'go',
      'rs': 'rust',
      'md': 'markdown',
      'html': 'html',
      'htm': 'html',
      'css': 'css',
      'scss': 'css',
      'less': 'css',
      'json': 'json',
      'json5': 'json',
      'xml': 'xml',
      'yaml': 'yaml',
      'yml': 'yaml',
      'txt': 'text',
      'sh': 'bash',
      'bat': 'batch'
    };
    const ext = extension.replace(/^\./, '').toLowerCase();
    return extMap[ext] || 'text';
  }
}

