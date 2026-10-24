export class WorkspaceVisualPolicy {
  static readonly minimumPointerTarget: number = 32;
  static readonly minimumUiFontSize: number = 10;
  static readonly compactToolbarHeight: number = 32;
  static readonly standardToolbarHeight: number = 34;
  static readonly compactIconSize: number = 16;
  static readonly standardIconSize: number = 18;
  static readonly compactRowHeight: number = 30;
  static readonly disclosureHeight: number = 34;
  static readonly disclosureIconSize: number = 14;
  static readonly panelTabHeight: number = 32;
  static readonly controlHorizontalPadding: number = 8;
  static readonly compactTouchTarget: number = 32;
  static readonly standardTouchTarget: number = 34;

  static menuTopInset(deviceType: string): number {
    return deviceType === 'tablet' ? 6 : deviceType === '2in1' ? 10 : 0;
  }
}
