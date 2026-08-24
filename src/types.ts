import type { Key, ReactNode } from "react"

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
  backgroundColor?: Color
  color?: Color
  fontSize?: number
  borderRadius?: number
  visibility?: "visible" | "hidden"
  overflow?: "visible" | "hidden" | "scroll"
}

export interface BlendxEvent {
  elementId: number
  eventType: "click" | "mouseDown" | "mouseUp" | "scroll"
  x: number
  y: number
  button: number
  deltaY: number
}

export interface HostProps {
  key?: Key | null
  children?: ReactNode
  style?: Style
  onClick?: (event: BlendxEvent) => void
  onMouseDown?: (event: BlendxEvent) => void
  onMouseUp?: (event: BlendxEvent) => void
  onScroll?: (event: BlendxEvent) => void
  /** Uniform row height used by `virtual-list`. */
  itemHeight?: number
  /** Extra rows painted above and below the viewport. */
  overdraw?: number
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
  frameMaxMs: number
  threads: number
}

export type NativeMutation =
  | ["create", number, string]
  | ["append", number, number]
  | ["remove", number, number]
  | ["insert", number, number, number]
  | ["style", number, Style]
  | ["text", number, string]
  | ["event", number, string, boolean]
  | ["prop", number, string, string | number | boolean | null]
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
  setCustomProp(id: number, name: string, value: string | number | boolean | null): void
  setEventListener(id: number, eventType: string, enabled: boolean): void
  setRoot(id: number): void
  commitMutations(): void
  applyBatch(mutations: NativeMutation[]): void
  setEventCallback(callback: (event: BlendxEvent) => void): void
  poll(): boolean
  renderFrame(): void
  getStats(): NativeStats
}

export interface BlendxRoot {
  render(node: ReactNode): void
  unmount(): void
  stop(): void
  renderer: NativeRenderer
}
