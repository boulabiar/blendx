import React from "react"
import ReactReconciler from "react-reconciler"
import ReconcilerConstants from "react-reconciler/constants.js"
import { dispatchEvent, registerEvent, unregisterEvent } from "./event-registry.js"
import { loadNativeRenderer } from "./native.js"
import type {
  BlendxEvent,
  BlendxRoot,
  BlendxElement,
  HostProps,
  NativeRenderer,
  NativeMutation,
  WindowOptions,
} from "./types.js"

export type {
  BlendxEvent,
  BlendxElement,
  BlendxRoot,
  Color,
  Length,
  CanvasCommand,
  AnchorPosition,
  HostProps,
  NativeStats,
  AccessibilityNode,
  Style,
  WindowOptions,
} from "./types.js"

export * from "./components/index.js"

type Instance = { id: number; type: string; props: HostProps }
type TextInstance = { id: number; text: string }
type Container = { renderer: NativeRenderer }

const { ConcurrentRoot, DefaultEventPriority } = ReconcilerConstants

let nextElementId = 0
let activeRenderer: NativeRenderer | null = null
let currentPriority = 0
const instances = new Map<number, Instance>()
const hoveredInstances = new Set<number>()
const activeInstances = new Set<number>()
let lastBridgeTimeMs = 0
let reactCommitStartedAt = 0
let lastReactCommitTimeMs = 0

function native(): NativeRenderer {
  if (!activeRenderer) throw new Error("BlendX renderer is not active")
  return activeRenderer
}

const eventProps = [
  ["onClick", "click"],
  ["onMouseDown", "mouseDown"],
  ["onMouseUp", "mouseUp"],
  ["onMouseMove", "mouseMove"],
  ["onMouseEnter", "mouseEnter"],
  ["onMouseLeave", "mouseLeave"],
  ["onMouseDownOutside", "mouseDownOutside"],
  ["onScroll", "scroll"],
  ["onChange", "change"],
  ["onSubmit", "submit"],
  ["onKeyDown", "keyDown"],
  ["onKeyUp", "keyUp"],
  ["onFocus", "focus"],
  ["onBlur", "blur"],
] as const

function createBatchedRenderer(raw: NativeRenderer): NativeRenderer {
  let pending: NativeMutation[] = []
  const flush = (): boolean => {
    if (pending.length === 0) return false
    const started = performance.now()
    raw.applyBatch(pending)
    lastBridgeTimeMs = performance.now() - started
    pending = []
    return true
  }
  return {
    init: raw.init.bind(raw),
    shutdown: raw.shutdown.bind(raw),
    createElement: (id, type) => pending.push(["create", id, type]),
    destroyElement: (id) => { flush(); return raw.destroyElement(id) },
    appendChild: (parent, child) => pending.push(["append", parent, child]),
    removeChild: (parent, child) => pending.push(["remove", parent, child]),
    insertBefore: (parent, child, before) => pending.push(["insert", parent, child, before]),
    setStyle: (id, style) => pending.push(["style", id, style]),
    setStylePatch: (id, patch) => pending.push(["stylePatch", id, ...patch]),
    setText: (id, text) => pending.push(["text", id, text]),
    setCustomProp: (id, name, value) => pending.push(["prop", id, name, value]),
    setEventListener: (id, type, enabled) => pending.push(["event", id, type, enabled]),
    setRoot: (id) => pending.push(["root", id]),
    commitMutations: () => { if (flush()) raw.commitMutations() },
    applyBatch: raw.applyBatch.bind(raw),
    setEventCallback: raw.setEventCallback.bind(raw),
    poll: raw.poll.bind(raw),
    renderFrame: raw.renderFrame.bind(raw),
    getStats: () => ({
      ...raw.getStats(),
      bridgeTimeMs: lastBridgeTimeMs,
      reactCommitTimeMs: lastReactCommitTimeMs,
    }),
    focusElement: raw.focusElement.bind(raw),
    dispatchPointer: raw.dispatchPointer.bind(raw),
    dispatchKey: raw.dispatchKey.bind(raw),
    scrollToItem: raw.scrollToItem.bind(raw),
    scrollToOffset: raw.scrollToOffset.bind(raw),
    getElementBox: raw.getElementBox.bind(raw),
    captureScreenshot: raw.captureScreenshot.bind(raw),
    getSelectedText: raw.getSelectedText.bind(raw),
    getAccessibilityTree: raw.getAccessibilityTree.bind(raw),
    nextFrameDelay: raw.nextFrameDelay.bind(raw),
  }
}

function hasPseudoListener(props: HostProps | null, eventType: string): boolean {
  if (!props) return false
  if (eventType === "mouseEnter" || eventType === "mouseLeave") return Boolean(props.style?.hover)
  if (eventType === "mouseDown" || eventType === "mouseUp") return Boolean(props.style?.active)
  return false
}

function syncEvents(id: number, oldProps: HostProps | null, props: HostProps): void {
  for (const [propName, eventType] of eventProps) {
    const oldHandler = oldProps?.[propName]
    const handler = props[propName]
    if (oldHandler && !handler) {
      unregisterEvent(id, eventType)
    } else if (handler && handler !== oldHandler) {
      registerEvent(id, eventType, handler as (event: BlendxEvent) => void)
    }
    const wasEnabled = Boolean(oldHandler) || hasPseudoListener(oldProps, eventType)
    const enabled = Boolean(handler) || hasPseudoListener(props, eventType)
    if (wasEnabled !== enabled) native().setEventListener(id, eventType, enabled)
  }
}

function resolvedStyle(id: number, style: HostProps["style"]): HostProps["style"] {
  if (!style) return style
  const { hover, active, ...base } = style
  if (activeInstances.has(id) && active) return { ...base, ...hover, ...active }
  if (hoveredInstances.has(id) && hover) return { ...base, ...hover }
  return base
}

function applyPointerStyle(renderer: NativeRenderer, event: BlendxEvent): void {
  const instance = instances.get(event.elementId)
  if (!instance) return
  if (event.eventType === "mouseEnter") hoveredInstances.add(instance.id)
  else if (event.eventType === "mouseLeave") {
    hoveredInstances.delete(instance.id)
    activeInstances.delete(instance.id)
  } else if (event.eventType === "mouseDown") activeInstances.add(instance.id)
  else if (event.eventType === "mouseUp") activeInstances.delete(instance.id)
  else return
  renderer.setStyle(instance.id, resolvedStyle(instance.id, instance.props.style) ?? {})
  renderer.commitMutations()
}

function primitiveText(props: HostProps): string | null {
  const { children } = props
  if (typeof children === "string" || typeof children === "number") return String(children)
  if (
    Array.isArray(children) &&
    children.every((child) => typeof child === "string" || typeof child === "number")
  ) {
    return children.map(String).join("")
  }
  return null
}

function stylesEqual(a: HostProps["style"], b: HostProps["style"]): boolean {
  if (a === b) return true
  if (!a || !b) return !a && !b
  const aKeys = Object.keys(a) as Array<keyof typeof a>
  const bKeys = Object.keys(b)
  return aKeys.length === bKeys.length && aKeys.every((key) => a[key] === b[key])
}

const stylePropertyIds: Partial<Record<keyof NonNullable<HostProps["style"]>, number>> = {
  height: 0,
  padding: 1,
  gap: 2,
  backgroundColor: 3,
  borderColor: 4,
  color: 5,
  opacity: 6,
  borderRadius: 7,
  width: 8,
  borderWidth: 9,
  fontSize: 10,
  lineHeight: 11,
  flexGrow: 12,
  flexShrink: 13,
  layoutContain: 14,
}

function encodeStylePatch(
  previous: HostProps["style"],
  next: HostProps["style"],
): unknown[] | null {
  if (!previous || !next) return null
  const previousKeys = Object.keys(previous) as Array<keyof NonNullable<typeof previous>>
  const nextKeys = Object.keys(next) as Array<keyof NonNullable<typeof next>>
  if (previousKeys.length !== nextKeys.length ||
      previousKeys.some((key) => !Object.prototype.hasOwnProperty.call(next, key))) return null
  const patch: unknown[] = []
  for (const key of nextKeys) {
    if (previous[key] === next[key]) continue
    if (key === "padding" && (next.paddingHorizontal !== undefined || next.paddingVertical !== undefined ||
        next.paddingLeft !== undefined || next.paddingRight !== undefined ||
        next.paddingTop !== undefined || next.paddingBottom !== undefined)) return null
    const propertyId = stylePropertyIds[key]
    if (propertyId === undefined) return null
    patch.push(propertyId, next[key])
  }
  return patch
}

function syncCustomProps(id: number, oldProps: HostProps | null, props: HostProps): void {
  for (const name of [
    "itemHeight", "overdraw", "estimatedItemHeight", "alignment", "followTail", "src", "alt", "objectFit",
    "commands", "source", "code", "language", "showLineNumbers", "showHeader",
    "patch", "wordDiff", "value", "max", "animationDurationMs", "animationLoop", "animationAlternate",
    "animateValue", "animateOpacity", "placeholder", "readOnly", "password", "selectable", "minRows",
    "maxRows", "autoFocus", "position", "side", "align", "anchor", "offset",
    "anchorGap", "anchorId", "tabIndex", "disabled", "modal", "accessibilityRole",
    "accessibilityLabel", "accessibilityDescription", "accessibilityValue",
    "accessibilityChecked", "accessibilitySelected",
  ] as const) {
    if (props[name] !== oldProps?.[name]) native().setCustomProp(id, name, props[name] ?? null)
  }
}

const hostConfig = {
  supportsMutation: true,
  supportsPersistence: false,
  supportsHydration: false,
  isPrimaryRenderer: true,

  createInstance(type: string, props: HostProps): Instance {
    const instance = { id: ++nextElementId, type, props }
    instances.set(instance.id, instance)
    native().createElement(instance.id, type)
    native().setStyle(instance.id, resolvedStyle(instance.id, props.style) ?? {})
    const content = primitiveText(props)
    if (type === "text" && content !== null) native().setText(instance.id, content)
    syncEvents(instance.id, null, props)
    syncCustomProps(instance.id, null, props)
    return instance
  },

  createTextInstance(text: string): TextInstance {
    const instance = { id: ++nextElementId, text }
    native().createElement(instance.id, "text")
    native().setText(instance.id, text)
    return instance
  },

  appendInitialChild(parent: Instance, child: Instance | TextInstance): void {
    native().appendChild(parent.id, child.id)
  },
  appendChild(parent: Instance, child: Instance | TextInstance): void {
    native().appendChild(parent.id, child.id)
  },
  appendChildToContainer(container: Container, child: Instance): void {
    container.renderer.setRoot(child.id)
  },
  removeChild(parent: Instance, child: Instance | TextInstance): void {
    native().removeChild(parent.id, child.id)
  },
  removeChildFromContainer(_container: Container, child: Instance): void {
    for (const id of native().destroyElement(child.id)) {
      unregisterEvent(id)
      instances.delete(id)
      hoveredInstances.delete(id)
      activeInstances.delete(id)
    }
  },
  insertBefore(parent: Instance, child: Instance | TextInstance, before: Instance | TextInstance): void {
    native().insertBefore(parent.id, child.id, before.id)
  },
  insertInContainerBefore(): void {},

  prepareForCommit(): null { reactCommitStartedAt = performance.now(); return null },
  resetAfterCommit(): void {
    native().commitMutations()
    lastReactCommitTimeMs = performance.now() - reactCommitStartedAt
  },
  getRootHostContext(): Record<string, never> { return {} },
  getChildHostContext(): Record<string, never> { return {} },
  getPublicInstance(instance: Instance): Instance { return instance },
  preparePortalMount(): void {},
  clearContainer(): void {},

  shouldSetTextContent(type: string, props: HostProps): boolean {
    return type === "text" && primitiveText(props) !== null
  },
  finalizeInitialChildren(): boolean { return false },
  commitMount(): void {},
  commitUpdate(instance: Instance, _type: string, oldProps: HostProps, newProps: HostProps): void {
    if (!stylesEqual(oldProps.style, newProps.style)) {
      const previous = resolvedStyle(instance.id, oldProps.style)
      const next = resolvedStyle(instance.id, newProps.style)
      const patch = encodeStylePatch(previous, next)
      if (patch === null) native().setStyle(instance.id, next ?? {})
      else if (patch.length > 0) native().setStylePatch(instance.id, patch)
    }
    syncEvents(instance.id, oldProps, newProps)
    syncCustomProps(instance.id, oldProps, newProps)
    const oldText = primitiveText(oldProps)
    const newText = primitiveText(newProps)
    if (instance.type === "text" && newText !== oldText) native().setText(instance.id, newText ?? "")
    instance.props = newProps
  },
  commitTextUpdate(instance: TextInstance, _oldText: string, text: string): void {
    native().setText(instance.id, text)
    instance.text = text
  },
  hideInstance(instance: Instance): void {
    native().setStyle(instance.id, { ...(instance.props.style ?? {}), visibility: "hidden" })
  },
  unhideInstance(instance: Instance): void { native().setStyle(instance.id, resolvedStyle(instance.id, instance.props.style) ?? {}) },
  hideTextInstance(): void {},
  unhideTextInstance(): void {},
  detachDeletedInstance(instance: Instance): void {
    for (const id of native().destroyElement(instance.id)) {
      unregisterEvent(id)
      instances.delete(id)
      hoveredInstances.delete(id)
      activeInstances.delete(id)
    }
  },

  scheduleTimeout: setTimeout,
  cancelTimeout: clearTimeout,
  noTimeout: -1,
  shouldAttemptEagerTransition(): boolean { return false },
  setCurrentUpdatePriority(priority: number): void { currentPriority = priority },
  getCurrentUpdatePriority(): number { return currentPriority },
  resolveUpdatePriority(): number { return currentPriority || DefaultEventPriority },
  maySuspendCommit(): boolean { return false },
  NotPendingTransition: null,
  HostTransitionContext: React.createContext(null),
  resetFormInstance(): void {},
  requestPostPaintCallback(): void {},
  trackSchedulerEvent(): void {},
  resolveEventType(): null { return null },
  resolveEventTimeStamp(): number { return -1 },
  preloadInstance(): boolean { return true },
  startSuspendingCommit(): void {},
  suspendInstance(): void {},
  waitForCommitToBeReady(): null { return null },
  getInstanceFromNode(): null { return null },
  beforeActiveInstanceBlur(): void {},
  afterActiveInstanceBlur(): void {},
  prepareScopeUpdate(): void {},
  getInstanceFromScope(): null { return null },
}

// react-reconciler's published types lag its host-config surface.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const reconciler = ReactReconciler(hostConfig as any)
const reconcilerWithSync = reconciler as typeof reconciler & {
  flushSyncFromReconciler?: (callback: () => void) => void
}

/** Coalesce state updates originating outside React events into one render pass. */
export function batchUpdates<T>(callback: () => T): T {
  return reconciler.batchedUpdates(callback, undefined)
}

function reportRenderError(error: unknown, info?: { componentStack?: string }): void {
  const detail = error && typeof error === "object" && "stack" in error
    ? String((error as { stack?: unknown }).stack ?? error)
    : String(error)
  console.error(`${detail}${info?.componentStack ?? ""}`)
}

function flushSync(callback: () => void): void {
  const flush = reconcilerWithSync.flushSyncFromReconciler ?? reconciler.flushSync
  flush(callback)
}

export function render(node: React.ReactNode, options: WindowOptions = {}): BlendxRoot {
  if (activeRenderer) {
    throw new Error("BlendX supports one live root per process; stop the existing root before rendering another")
  }
  const rawRenderer = loadNativeRenderer()
  rawRenderer.init(options)
  const renderer = createBatchedRenderer(rawRenderer)
  activeRenderer = renderer
  nextElementId = 0

  const containerInfo: Container = { renderer }
  // The signature intentionally follows React 19 / react-reconciler 0.33.
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const container = (reconciler.createContainer as any)(
    containerInfo,
    ConcurrentRoot,
    null,
    false,
    null,
    "blendx",
    reportRenderError,
    reportRenderError,
    reportRenderError,
    null
  )

  renderer.setEventCallback((event) => {
    applyPointerStyle(renderer, event)
    flushSync(() => dispatchEvent(event))
  })
  flushSync(() => reconciler.updateContainer(node, container, null, () => {}))

  let stopped = false
  let timer: ReturnType<typeof setTimeout> | null = null
  const stop = (): void => {
    if (stopped) return
    stopped = true
    if (timer) clearTimeout(timer)
    flushSync(() => reconciler.updateContainer(null, container, null, () => {}))
    renderer.shutdown()
    if (activeRenderer === renderer) activeRenderer = null
    instances.clear()
    hoveredInstances.clear()
    activeInstances.clear()
  }
  const tick = (): void => {
    if (stopped) return
    if (!renderer.poll()) {
      stop()
      return
    }
    timer = setTimeout(tick, 8)
  }
  if (!(globalThis as typeof globalThis & { __blendxNativeEventLoop?: boolean }).__blendxNativeEventLoop) tick()

  return {
    renderer,
    render(nextNode): void {
      activeRenderer = renderer
      flushSync(() => reconciler.updateContainer(nextNode, container, null, () => {}))
    },
    unmount: stop,
    stop,
  }
}
