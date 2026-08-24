import type { BlendxEvent } from "./types.js"

const handlers = new Map<number, Map<string, (event: BlendxEvent) => void>>()

export function registerEvent(
  id: number,
  type: string,
  handler: (event: BlendxEvent) => void
): void {
  let byType = handlers.get(id)
  if (!byType) {
    byType = new Map()
    handlers.set(id, byType)
  }
  byType.set(type, handler)
}

export function unregisterEvent(id: number, type?: string): void {
  if (type === undefined) {
    handlers.delete(id)
    return
  }
  const byType = handlers.get(id)
  byType?.delete(type)
  if (byType?.size === 0) handlers.delete(id)
}

export function dispatchEvent(event: BlendxEvent): void {
  handlers.get(event.elementId)?.get(event.eventType)?.(event)
}
