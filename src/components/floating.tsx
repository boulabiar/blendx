import React from "react"
import type { BlendxElement, HostProps, Style } from "../types.js"

export type StateStyle<State> = Style | ((state: State) => Style)

export function resolveStyle<State>(style: StateStyle<State> | undefined, state: State): Style | undefined {
  return typeof style === "function" ? style(state) : style
}

export function setRefs<T>(value: T, ...refs: Array<React.Ref<T> | undefined>): void {
  for (const ref of refs) {
    if (typeof ref === "function") ref(value)
    else if (ref) (ref as React.MutableRefObject<T>).current = value
  }
}

export function useControllableState<T>({
  value,
  defaultValue,
  onChange,
}: {
  value?: T
  defaultValue: T
  onChange?: (value: T) => void
}): [T, (value: T) => void] {
  const [internal, setInternal] = React.useState(defaultValue)
  const controlled = value !== undefined
  const current = controlled ? value : internal
  const set = React.useCallback((next: T) => {
    if (!controlled) setInternal(next)
    if (!Object.is(current, next)) onChange?.(next)
  }, [controlled, current, onChange])
  return [current, set]
}

export function normalizeKey(key?: string): string {
  if (!key) return ""
  return key.replace(/^Arrow/, "").toLowerCase()
}

export interface FloatingContentProps extends HostProps {
  anchorId?: number
  side?: "top" | "right" | "bottom" | "left"
  align?: "start" | "center" | "end"
  sideOffset?: number
}

export const FloatingLayer = React.forwardRef<BlendxElement, FloatingContentProps>(
  function FloatingLayer({ sideOffset = 4, style, ...props }, ref) {
    return (
      <anchored
        {...props}
        ref={ref}
        anchorGap={sideOffset}
        style={{ position: "absolute", ...style }}
      />
    )
  },
)
