export enum ResizeAxis {
  Horizontal = 'horizontal',
  Vertical = 'vertical'
}

@Component
export struct ResizablePane {
  @Prop axis: ResizeAxis = ResizeAxis.Horizontal;
  @Prop horizontalThickness: number = 6;
  @Prop verticalThickness: number = 18;
  @Prop frameIntervalMilliseconds: number = 16;
  @Prop darkMode: boolean = false;
  onResizeStart: () => void = () => {};
  onResizeUpdate: (offset: number) => void = (_offset: number) => {};
  onResizeEnd: () => void = () => {};
  onResizeCancel: () => void = () => {};
  onReset: () => void = () => {};
  onHoverChanged: (hovered: boolean) => void = (_hovered: boolean) => {};

  @State private hovered: boolean = false;
  @State private dragging: boolean = false;
  private lastDispatchAt: number = 0;
  private latestOffset: number = 0;
  private lastClickAt: number = 0;

  private gestureOffset(event: GestureEvent): number {
    return this.axis === ResizeAxis.Horizontal ? event.offsetX : event.offsetY;
  }

  private dispatchLatest(force: boolean): void {
    const now = Date.now();
    if (!force && now - this.lastDispatchAt < this.frameIntervalMilliseconds) return;
    this.lastDispatchAt = now;
    this.onResizeUpdate(this.latestOffset);
  }

  private handleClick(): void {
    const now = Date.now();
    if (now - this.lastClickAt <= 360) {
      this.lastClickAt = 0;
      this.onReset();
      return;
    }
    this.lastClickAt = now;
  }

  build() {
    Row() {}
      .width(this.axis === ResizeAxis.Horizontal ? this.horizontalThickness : '100%')
      .height(this.axis === ResizeAxis.Vertical ? this.verticalThickness : '100%')
      .backgroundColor(this.hovered || this.dragging ?
        (this.darkMode ? $r('app.color.workspace_dark_surface_hover') :
          $r('app.color.workspace_light_surface_hover')) :
        (this.darkMode ? $r('app.color.workspace_dark_divider') :
          $r('app.color.workspace_light_divider')))
      .focusable(true)
      .accessibilityText('拖动调整面板大小，双击恢复默认')
      .responseRegion({ x: -3, y: -3, width: '100%', height: '100%' })
      .onHover((isHover: boolean) => {
        this.hovered = isHover;
        this.onHoverChanged(isHover);
      })
      .onClick(() => { this.handleClick(); })
      .gesture(
        PanGesture()
          .onActionStart(() => {
            this.dragging = true;
            this.latestOffset = 0;
            this.lastDispatchAt = 0;
            this.onResizeStart();
          })
          .onActionUpdate((event: GestureEvent) => {
            this.latestOffset = this.gestureOffset(event);
            this.dispatchLatest(false);
          })
          .onActionEnd(() => {
            this.dispatchLatest(true);
            this.dragging = false;
            this.onResizeEnd();
          })
          .onActionCancel(() => {
            this.dragging = false;
            this.onResizeCancel();
          })
      )
  }
}
