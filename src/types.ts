import type { Key, ReactNode, Ref } from "react"

export type Color = `#${string}`
export type Length = number | `${number}%`

export interface Style {
  width?: Length
  height?: Length
  minWidth?: number
  minHeight?: number
  flexDirection?: "row" | "column"
  flexGrow?: number
  gap?: number
  padding?: number
  paddingHorizontal?: number
  paddingVertical?: number
  paddingLeft?: number
  paddingRight?: number
  paddingTop?: number
  paddingBottom?: number
  marginLeft?: number
  marginRight?: number
  marginTop?: number
  marginBottom?: number
  backgroundColor?: Color
  color?: Color
  fontSize?: number
  lineHeight?: number
  fontWeight?: number
  borderRadius?: number
  borderWidth?: number
  borderColor?: Color
  opacity?: number
  visibility?: "visible" | "hidden"
  overflow?: "visible" | "hidden" | "scroll"
  overflowY?: "visible" | "hidden" | "scroll"
  position?: "relative" | "absolute" | "fixed"
  left?: number
  right?: number
  top?: number
  bottom?: number
  /** Paint order within the parent; larger values are drawn on top. */
  zIndex?: number
  maxWidth?: Length
  maxHeight?: Length
  flexShrink?: number
  flexWrap?: "nowrap" | "wrap"
  alignItems?: "start" | "center" | "end" | "flex-start" | "flex-end" | "stretch"
  justifyContent?: "start" | "center" | "end" | "flex-start" | "flex-end" | "spaceBetween" | "space-between"
  cursor?: "default" | "pointer" | "text"
  whiteSpace?: "normal" | "nowrap" | "pre" | "preWrap"
  textOverflow?: "clip" | "ellipsis"
  /** Native pointer-state overrides. */
  hover?: Omit<Style, "hover" | "active">
  active?: Omit<Style, "hover" | "active">
}

export interface BlendxEvent {
  elementId: number
  eventType:
    | "click"
    | "mouseDown"
    | "mouseUp"
    | "mouseMove"
    | "mouseEnter"
    | "mouseLeave"
    | "mouseDownOutside"
    | "scroll"
    | "change"
    | "submit"
    | "keyDown"
    | "keyUp"
    | "focus"
    | "blur"
  x: number
  y: number
  button: number
  deltaY: number
  scrollOffset?: number
  scrollTarget?: number
  viewportSize?: number
  contentSize?: number
  value?: string
  key?: string
}

export interface BlendxElement {
  id: number
  type: string
}

export type CanvasCommand =
  | { kind: "fillRect"; x: number; y: number; width: number; height: number; color: Color; radius?: number }
  | { kind: "strokeRect"; x: number; y: number; width: number; height: number; color: Color; widthPx?: number; radius?: number }
  | { kind: "line"; x1: number; y1: number; x2: number; y2: number; color: Color; widthPx?: number }
  | { kind: "circle"; x: number; y: number; radius: number; color: Color; fill?: boolean; widthPx?: number }
  | { kind: "text"; x: number; y: number; text: string; color: Color; fontSize?: number }

export interface AnchorPosition { x: number; y: number }

export interface HostProps {
  key?: Key | null
  ref?: Ref<BlendxElement>
  children?: ReactNode
  style?: Style
  onClick?: (event: BlendxEvent) => void
  onMouseDown?: (event: BlendxEvent) => void
  onMouseUp?: (event: BlendxEvent) => void
  onMouseMove?: (event: BlendxEvent) => void
  onMouseEnter?: (event: BlendxEvent) => void
  onMouseLeave?: (event: BlendxEvent) => void
  onMouseDownOutside?: (event: BlendxEvent) => void
  onScroll?: (event: BlendxEvent) => void
  onChange?: (event: BlendxEvent) => void
  onSubmit?: (event: BlendxEvent) => void
  onKeyDown?: (event: BlendxEvent) => void
  onKeyUp?: (event: BlendxEvent) => void
  onFocus?: (event: BlendxEvent) => void
  onBlur?: (event: BlendxEvent) => void
  /** Uniform row height used by `virtual-list`. */
  itemHeight?: number
  /** Extra rows painted above and below the viewport. */
  overdraw?: number
  estimatedItemHeight?: number
  alignment?: "top" | "bottom"
  followTail?: boolean
  src?: string
  alt?: string
  objectFit?: "fill" | "contain" | "cover" | "scaleDown" | "none"
  commands?: CanvasCommand[]
  source?: string
  code?: string
  language?: string
  showLineNumbers?: boolean
  showHeader?: boolean
  patch?: string
  wordDiff?: boolean
  value?: string | number
  max?: number
  placeholder?: string
  readOnly?: boolean
  password?: boolean
  selectable?: boolean
  minRows?: number
  maxRows?: number
  autoFocus?: boolean
  tabIndex?: number
  disabled?: boolean
  modal?: boolean
  accessibilityRole?: "button" | "checkbox" | "dialog" | "heading" | "image" | "list" | "listitem" | "menu" | "menuitem" | "radio" | "slider" | "status" | "switch" | "tab" | "textbox"
  accessibilityLabel?: string
  accessibilityDescription?: string
  accessibilityValue?: string
  accessibilityChecked?: boolean | "mixed"
  accessibilitySelected?: boolean
  position?: AnchorPosition
  side?: "top" | "right" | "bottom" | "left"
  align?: "start" | "center" | "end"
  anchor?: "topLeft" | "topCenter" | "topRight" | "rightCenter" | "bottomRight" | "bottomCenter" | "bottomLeft" | "leftCenter"
  offset?: AnchorPosition
  anchorGap?: number
  /** Native element ID used as the overlay anchor. */
  anchorId?: number
}

export interface WindowOptions {
  title?: string
  width?: number
  height?: number
  threads?: number
  fontPath?: string
  headless?: boolean
}

export interface NativeStats {
  width: number
  height: number
  nodeCount: number
  frameCount: number
  renderTimeMs: number
  layoutTimeMs: number
  paintTimeMs: number
  presentTimeMs: number
  paintedPixels: number
  paintedNodes: number
  dirtyRectCount: number
  mutationsLastCommit: number
  frameP50Ms: number
  frameP95Ms: number
  frameP99Ms: number
  frameMaxMs: number
  /** Frames over the 60 Hz 16.67 ms budget in the rolling sample window. */
  framesOverBudget: number
  threads: number
}

export interface AccessibilityNode {
  id: number
  role: string
  label: string
  description: string
  value: string
  checked: "" | "true" | "false" | "mixed"
  disabled: boolean
  selected: boolean
  x: number
  y: number
  width: number
  height: number
}

export type NativeMutation =
  | ["create", number, string]
  | ["append", number, number]
  | ["remove", number, number]
  | ["insert", number, number, number]
  | ["style", number, Style]
  | ["text", number, string]
  | ["event", number, string, boolean]
  | ["prop", number, string, unknown]
  | ["root", number]

export interface NativeRenderer {
  init(options?: WindowOptions): void
  shutdown(): void
  createElement(id: number, type: string): void
  destroyElement(id: number): number[]
  appendChild(parentId: number, childId: number): void
  removeChild(parentId: number, childId: number): void
  insertBefore(parentId: number, childId: number, beforeId: number): void
  setStyle(id: number, style: Style): void
  setText(id: number, text: string): void
  setCustomProp(id: number, name: string, value: unknown): void
  setEventListener(id: number, eventType: string, enabled: boolean): void
  setRoot(id: number): void
  commitMutations(): void
  applyBatch(mutations: NativeMutation[]): void
  setEventCallback(callback: (event: BlendxEvent) => void): void
  poll(): boolean
  renderFrame(): void
  getStats(): NativeStats
  focusElement(id: number): void
  /** Headless/native automation helpers used by integration tests. */
  dispatchPointer(kind: "mouseMove" | "mouseDown" | "mouseUp" | "click", x: number, y: number, button?: number): void
  dispatchKey(key: string): void
  scrollToItem(id: number, index: number): void
  scrollToOffset(id: number, offset: number): void
  getElementBox(id: number): { x: number; y: number; width: number; height: number }
  captureScreenshot(path: string): void
  getSelectedText(): string
  getAccessibilityTree(): AccessibilityNode[]
}

export interface BlendxRoot {
  render(node: ReactNode): void
  unmount(): void
  stop(): void
  renderer: NativeRenderer
}
