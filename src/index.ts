import React, { createContext } from "react"
import ReactReconciler from "react-reconciler"
import { ConcurrentRoot, DefaultEventPriority } from "react-reconciler/constants.js"
import { dispatchEvent, registerEvent, unregisterEvent } from "./event-registry.js"
import { loadNativeRenderer } from "./native.js"
import type {
  BlendxEvent,
  BlendxRoot,
  HostProps,
  NativeRenderer,
  NativeMutation,
  WindowOptions,
} from "./types.js"

export type {
  BlendxEvent,
  BlendxRoot,
  HostProps,
  NativeStats,
  Style,
  WindowOptions,
} from "./types.js"

type Instance = { id: number; type: string; props: HostProps }
type TextInstance = { id: number; text: string }
type Container = { renderer: NativeRenderer }

let nextElementId = 0
let activeRenderer: NativeRenderer | null = null
let currentPriority = 0

function native(): NativeRenderer {
  if (!activeRenderer) throw new Error("BlendX renderer is not active")
  return activeRenderer
}

const eventProps = [
  ["onClick", "click"],
  ["onMouseDown", "mouseDown"],
  ["onMouseUp", "mouseUp"],
  ["onScroll", "scroll"],
] as const

function createBatchedRenderer(raw: NativeRenderer): NativeRenderer {
  let pending: NativeMutation[] = []
  const flush = (): boolean => {
    if (pending.length === 0) return false
    raw.applyBatch(pending)
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
    setText: (id, text) => pending.push(["text", id, text]),
    setCustomProp: (id, name, value) => pending.push(["prop", id, name, value]),
    setEventListener: (id, type, enabled) => pending.push(["event", id, type, enabled]),
    setRoot: (id) => pending.push(["root", id]),
    commitMutations: () => { if (flush()) raw.commitMutations() },
    applyBatch: raw.applyBatch.bind(raw),
    setEventCallback: raw.setEventCallback.bind(raw),
    poll: raw.poll.bind(raw),
    renderFrame: raw.renderFrame.bind(raw),
    getStats: raw.getStats.bind(raw),
  }
}

function syncEvents(id: number, oldProps: HostProps | null, props: HostProps): void {
  for (const [propName, eventType] of eventProps) {
    const oldHandler = oldProps?.[propName]
    const handler = props[propName]
    if (oldHandler && !handler) {
      unregisterEvent(id, eventType)
      native().setEventListener(id, eventType, false)
    } else if (handler && handler !== oldHandler) {
      registerEvent(id, eventType, handler as (event: BlendxEvent) => void)
      if (!oldHandler) native().setEventListener(id, eventType, true)
    }
  }
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

function syncCustomProps(id: number, oldProps: HostProps | null, props: HostProps): void {
  for (const name of ["itemHeight", "overdraw"] as const) {
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
    native().createElement(instance.id, type)
    native().setStyle(instance.id, props.style ?? {})
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
    for (const id of native().destroyElement(child.id)) unregisterEvent(id)
  },
  insertBefore(parent: Instance, child: Instance | TextInstance, before: Instance | TextInstance): void {
    native().insertBefore(parent.id, child.id, before.id)
  },
  insertInContainerBefore(): void {},

  prepareForCommit(): null { return null },
  resetAfterCommit(): void { native().commitMutations() },
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
      native().setStyle(instance.id, newProps.style ?? {})
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
  unhideInstance(instance: Instance): void { native().setStyle(instance.id, instance.props.style ?? {}) },
  hideTextInstance(): void {},
  unhideTextInstance(): void {},
  detachDeletedInstance(instance: Instance): void {
    for (const id of native().destroyElement(instance.id)) unregisterEvent(id)
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
  HostTransitionContext: createContext(null),
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

function flushSync(callback: () => void): void {
  const flush = reconcilerWithSync.flushSyncFromReconciler ?? reconciler.flushSync
  flush(callback)
}

export function render(node: React.ReactNode, options: WindowOptions = {}): BlendxRoot {
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
    console.error,
    console.error,
    console.error,
    null
  )

  renderer.setEventCallback((event) => {
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
  }
  const tick = (): void => {
    if (stopped) return
    if (!renderer.poll()) {
      stop()
      return
    }
    timer = setTimeout(tick, 8)
  }
  tick()

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
