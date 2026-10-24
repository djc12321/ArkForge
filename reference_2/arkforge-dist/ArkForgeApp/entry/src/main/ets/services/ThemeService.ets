/**
 * 主题服务
 * 管理应用的主题切换和颜色配置
 *
 * 来源：KeloYuan/xxCode（MIT License）https://github.com/KeloYuan/xxCode
 * 在 ArkForge 中新增了 `opencode` 主题，配色取自 OpenCode 设计系统
 * （暖深色画布 #201d1d / 4px 圆角 / 8px 网格 / Accent Blue #007aff），
 * 以贴近 Codex / DeepSeek Harness 一类 AI 编码界面的观感。
 */

export interface ThemeColors {
  background: string;
  surface: string;
  primary: string;
  secondary: string;
  text: string;
  textSecondary: string;
  border: string;
  error: string;
  success: string;
  warning: string;
}

export interface EditorColors {
  background: string;
  lineNumber: string;
  selection: string;
  cursor: string;
  text: string;
}

export interface SyntaxColors {
  keyword: string;
  string: string;
  comment: string;
  number: string;
  function: string;
  variable: string;
  operator: string;
  type: string;
}

export interface Theme {
  id: string;
  name: string;
  colors: ThemeColors;
  editor: EditorColors;
  syntax: SyntaxColors;
}

export class ThemeService {
  private static instance: ThemeService | null = null;
  private currentTheme: Theme;
  private themes: Map<string, Theme> = new Map();
  private listeners: ((theme: Theme) => void)[] = [];

  private constructor() {
    this.initializeThemes();
    this.currentTheme = this.themes.get('harmony-dark')!;
  }

  public static getInstance(): ThemeService {
    if (!ThemeService.instance) {
      ThemeService.instance = new ThemeService();
    }
    return ThemeService.instance;
  }

  /**
   * 初始化内置主题
   */
  private initializeThemes(): void {
    // === 鸿蒙6官方深色主题(默认) ===
    const harmonyDarkTheme: Theme = {
      id: 'harmony-dark',
      name: '鸿蒙深色',
      colors: {
        background: '#1e1e1e',
        surface: '#252526',
        primary: '#007acc',
        secondary: '#0091FF',
        text: '#E6E6E6',
        textSecondary: '#99999D',
        border: '#3c3c3c',
        error: '#FA2A2D',
        success: '#45D62B',
        warning: '#FF9500'
      },
      editor: {
        background: '#1e1e1e',
        lineNumber: '#5C5C60',
        selection: '#2D5DAF',
        cursor: '#007acc',
        text: '#E6E6E6'
      },
      syntax: {
        keyword: '#FC6CA3',
        string: '#8FD17F',
        comment: '#5C5C60',
        number: '#D79B8C',
        function: '#67AEFF',
        variable: '#E6E6E6',
        operator: '#6CECEF',
        type: '#DABAFF'
      }
    };
    this.themes.set('harmony-dark', harmonyDarkTheme);

    // === 鸿蒙6官方浅色主题 ===
    const harmonyLightTheme: Theme = {
      id: 'harmony-light',
      name: 'ArkForge 浅色',
      colors: {
        background: '#FFFFFF',
        surface: '#ECEFF3',
        primary: '#0A59F7',
        secondary: '#0091FF',
        text: '#1A1A1A',
        textSecondary: '#6D6D72',
        border: '#AAB4C2',
        error: '#FA2A2D',
        success: '#1EC050',
        warning: '#FF9500'
      },
      editor: {
        background: '#FFFFFF',
        lineNumber: '#8E8E93',
        selection: '#B3D7FF',
        cursor: '#0A59F7',
        text: '#1A1A1A'
      },
      syntax: {
        keyword: '#AD3DA4',
        string: '#1E8F4D',
        comment: '#6D6D72',
        number: '#C94E23',
        function: '#0E5EBE',
        variable: '#1A1A1A',
        operator: '#0083BC',
        type: '#5D37AF'
      }
    };
    this.themes.set('harmony-light', harmonyLightTheme);

    // === 优化后的深色主题 ===
    const darkTheme: Theme = {
      id: 'dark',
      name: '经典深色',
      colors: {
        background: '#1A1A1A',
        surface: '#252526',
        primary: '#5B8FF9',
        secondary: '#7C3AED',
        text: '#E8E8E8',
        textSecondary: '#A0A0A0',
        border: '#3E3E42',
        error: '#FF6B6B',
        success: '#51CF66',
        warning: '#FFD43B'
      },
      editor: {
        background: '#1E1E1E',
        lineNumber: '#7A7A7A',
        selection: '#3A3D41',
        cursor: '#5B8FF9',
        text: '#D4D4D4'
      },
      syntax: {
        keyword: '#C792EA',
        string: '#99C794',
        comment: '#676E95',
        number: '#F78C6C',
        function: '#82AAFF',
        variable: '#C5C8C6',
        operator: '#89DDFF',
        type: '#FFCB6B'
      }
    };
    this.themes.set('dark', darkTheme);

    // === 优化后的浅色主题 ===
    const lightTheme: Theme = {
      id: 'light',
      name: '经典浅色',
      colors: {
        background: '#FAFAFA',
        surface: '#FFFFFF',
        primary: '#3B82F6',
        secondary: '#8B5CF6',
        text: '#1F2937',
        textSecondary: '#6B7280',
        border: '#E5E7EB',
        error: '#EF4444',
        success: '#10B981',
        warning: '#F59E0B'
      },
      editor: {
        background: '#FFFFFF',
        lineNumber: '#9CA3AF',
        selection: '#DBEAFE',
        cursor: '#3B82F6',
        text: '#1F2937'
      },
      syntax: {
        keyword: '#9333EA',
        string: '#059669',
        comment: '#9CA3AF',
        number: '#EA580C',
        function: '#2563EB',
        variable: '#374151',
        operator: '#0891B2',
        type: '#7C3AED'
      }
    };
    this.themes.set('light', lightTheme);

    // === Monokai 主题（微调）===
    const monokaiTheme: Theme = {
      id: 'monokai',
      name: 'Monokai',
      colors: {
        background: '#272822',
        surface: '#3E3D32',
        primary: '#F92672',
        secondary: '#66D9EF',
        text: '#F8F8F2',
        textSecondary: '#E6DB74',
        border: '#49483E',
        error: '#F92672',
        success: '#A6E22E',
        warning: '#FD971F'
      },
      editor: {
        background: '#272822',
        lineNumber: '#90908A',
        selection: '#49483E',
        cursor: '#F8F8F0',
        text: '#F8F8F2'
      },
      syntax: {
        keyword: '#F92672',
        string: '#E6DB74',
        comment: '#75715E',
        number: '#AE81FF',
        function: '#A6E22E',
        variable: '#F8F8F2',
        operator: '#F92672',
        type: '#66D9EF'
      }
    };
    this.themes.set('monokai', monokaiTheme);

    // === Dracula 主题（优化对比度）===
    const draculaTheme: Theme = {
      id: 'dracula',
      name: 'Dracula',
      colors: {
        background: '#282A36',
        surface: '#343746',
        primary: '#BD93F9',
        secondary: '#FF79C6',
        text: '#F8F8F2',
        textSecondary: '#BFBFBF',
        border: '#44475A',
        error: '#FF5555',
        success: '#50FA7B',
        warning: '#FFB86C'
      },
      editor: {
        background: '#282A36',
        lineNumber: '#6272A4',
        selection: '#44475A',
        cursor: '#F8F8F0',
        text: '#F8F8F2'
      },
      syntax: {
        keyword: '#FF79C6',
        string: '#F1FA8C',
        comment: '#6272A4',
        number: '#BD93F9',
        function: '#50FA7B',
        variable: '#F8F8F2',
        operator: '#FF79C6',
        type: '#8BE9FD'
      }
    };
    this.themes.set('dracula', draculaTheme);

    // === OpenCode 主题（默认）：暖深色画布 + Accent Blue ===
    const opencodeTheme: Theme = {
      id: 'opencode',
      name: 'OpenCode',
      colors: {
        background: '#201d1d',
        surface: '#302c2c',
        primary: '#007aff',
        secondary: '#5ac8fa',
        text: '#fdfcfc',
        textSecondary: '#9a9898',
        border: '#3d3838',
        error: '#ff6b60',
        success: '#30d158',
        warning: '#ff9f0a'
      },
      editor: {
        background: '#201d1d',
        lineNumber: '#6f6a6a',
        selection: '#3d5a8a',
        cursor: '#007aff',
        text: '#fdfcfc'
      },
      syntax: {
        keyword: '#ff79c6',
        string: '#e6db74',
        comment: '#6f6a6a',
        number: '#bd93f9',
        function: '#50fa7b',
        variable: '#fdfcfc',
        operator: '#8be9fd',
        type: '#ffb86c'
      }
    };
    this.themes.set('opencode', opencodeTheme);
  }

  /**
   * 获取当前主题
   */
  public getCurrentTheme(): Theme {
    return this.currentTheme;
  }

  /**
   * 获取所有主题
   */
  public getAllThemes(): Theme[] {
    return Array.from(this.themes.values());
  }

  /**
   * 切换主题
   */
  public switchTheme(themeId: string): boolean {
    const theme = this.themes.get(themeId);
    if (!theme) {
      console.error('主题不存在:', themeId);
      return false;
    }

    this.currentTheme = theme;
    this.notifyListeners();
    return true;
  }

  /**
   * 添加主题变化监听器
   */
  public onThemeChange(callback: (theme: Theme) => void): void {
    this.listeners.push(callback);
  }

  /**
   * 移除主题变化监听器
   */
  public removeThemeChangeListener(callback: (theme: Theme) => void): void {
    const index = this.listeners.indexOf(callback);
    if (index > -1) {
      this.listeners.splice(index, 1);
    }
  }

  /**
   * 通知所有监听器
   */
  private notifyListeners(): void {
    this.listeners.forEach(listener => {
      try {
        listener(this.currentTheme);
      } catch (error) {
        console.error('主题监听器执行错误:', error);
      }
    });
  }

  /**
   * 保存主题偏好设置
   */
  public saveThemePreference(themeId: string): void {
    AppStorage.setOrCreate('selectedTheme', themeId);
  }

  /**
   * 加载主题偏好设置
   */
  public loadThemePreference(): void {
    const savedThemeId = AppStorage.get<string>('selectedTheme');
    if (savedThemeId && this.themes.has(savedThemeId)) {
      this.switchTheme(savedThemeId);
    }
  }

  /**
   * 获取下一个主题（用于主题切换循环）
   */
  public getNextTheme(): Theme {
    const themeIds = Array.from(this.themes.keys());
    const currentIndex = themeIds.indexOf(this.currentTheme.id);
    const nextIndex = (currentIndex + 1) % themeIds.length;
    return this.themes.get(themeIds[nextIndex])!;
  }
}

