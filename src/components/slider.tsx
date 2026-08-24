import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { BlendxElement, BlendxEvent, HostProps, Style } from "../types.js"
import { normalizeKey, resolveStyle, useControllableState } from "./floating.js"
import type { StateStyle } from "./floating.js"

export type SliderValue = number | [number, number]
export interface SliderState {
  value: SliderValue
  disabled: boolean
  focused: boolean
  dragging: boolean
}

export interface SliderProps extends Omit<HostProps, "children" | "style" | "value"> {
  value?: SliderValue
  defaultValue?: SliderValue
  onValueChange?: (value: SliderValue) => void
  min?: number
  max?: number
  step?: number
  style?: StateStyle<SliderState>
  trackStyle?: Style
  rangeStyle?: Style
  thumbStyle?: Style
}

function decimals(value: number): number {
  const text = String(value)
  return text.includes(".") ? text.length - text.indexOf(".") - 1 : 0
}

export const Slider = React.forwardRef<BlendxElement, SliderProps>(
  function Slider({ value: valueProp, defaultValue = 0, onValueChange, min = 0, max = 100, step = 1, disabled = false, style, trackStyle, rangeStyle, thumbStyle, onMouseDown, onMouseMove, onMouseUp, onKeyDown, onFocus, onBlur, ...props }, forwardedRef) {
    const [value, setValue] = useControllableState<SliderValue>({ value: valueProp, defaultValue, onChange: onValueChange })
    const [focused, setFocused] = React.useState(false)
    const [dragging, setDragging] = React.useState(false)
    const element = React.useRef<BlendxElement | null>(null)
    const activeThumb = React.useRef(0)
    const safeMax = Math.max(min + Math.abs(step || 1), max)
    const clamp = (next: number) => {
      const precision = Math.max(decimals(step), decimals(min))
      const snapped = min + Math.round((next - min) / Math.max(Math.abs(step), Number.EPSILON)) * Math.abs(step)
      return Number(Math.min(safeMax, Math.max(min, snapped)).toFixed(precision))
    }
    const values: [number, number] = Array.isArray(value) ? [clamp(value[0]), clamp(value[1])] : [min, clamp(value)]
    if (values[0] > values[1]) values.reverse()
    const commitAt = (event: BlendxEvent) => {
      if (disabled || !element.current) return
      const box = loadNativeRenderer().getElementBox(element.current.id)
      const ratio = Math.min(1, Math.max(0, (event.x - box.x) / Math.max(1, box.width)))
      const next = clamp(min + ratio * (safeMax - min))
      if (Array.isArray(value)) {
        const index = Math.abs(next - values[0]) <= Math.abs(next - values[1]) ? 0 : 1
        activeThumb.current = index
        const pair: [number, number] = [values[0], values[1]]
        pair[index] = next
        if (pair[0] > pair[1]) pair.reverse()
        setValue(pair)
      } else {
        setValue(next)
      }
    }
    const adjust = (delta: number) => {
      if (Array.isArray(value)) {
        const pair: [number, number] = [values[0], values[1]]
        pair[activeThumb.current] = clamp(pair[activeThumb.current] + delta)
        if (pair[0] > pair[1]) pair.reverse()
        setValue(pair)
      } else setValue(clamp(values[1] + delta))
    }
    const state = { value, disabled, focused, dragging }
    const lowPercent = ((values[0] - min) / (safeMax - min)) * 100
    const highPercent = ((values[1] - min) / (safeMax - min)) * 100
    const rangePercent = Math.max(0, highPercent - lowPercent)
    const thumb = { width: 16, height: 16, flexShrink: 0, zIndex: 1, borderRadius: 8, backgroundColor: "#f4f7fb", borderWidth: 2, borderColor: "#6f7cff", ...thumbStyle } as Style
    const setElement = (next: BlendxElement | null) => {
      element.current = next
      if (typeof forwardedRef === "function") forwardedRef(next)
      else if (forwardedRef) (forwardedRef as React.MutableRefObject<BlendxElement | null>).current = next
    }
    return (
      <button
        {...props}
        ref={setElement}
        disabled={disabled}
        tabIndex={props.tabIndex ?? 0}
        style={{ width: 240, height: 24, flexDirection: "row", alignItems: "center", position: "relative", ...(resolveStyle(style, state) ?? {}) }}
        onMouseDown={(event) => { if (disabled) return; onMouseDown?.(event); setDragging(true); commitAt(event) }}
        onMouseMove={(event) => { onMouseMove?.(event); if (dragging) commitAt(event) }}
        onMouseUp={(event) => { onMouseUp?.(event); setDragging(false) }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "right" || key === "up") adjust(Math.abs(step))
          else if (key === "left" || key === "down") adjust(-Math.abs(step))
          else if (key === "home") setValue(Array.isArray(value) ? [min, values[1]] : min)
          else if (key === "end") setValue(Array.isArray(value) ? [values[0], safeMax] : safeMax)
        }}
        onFocus={(event) => { onFocus?.(event); setFocused(true) }}
        onBlur={(event) => { onBlur?.(event); setFocused(false); setDragging(false) }}
      >
        <div style={{ position: "absolute", left: 0, right: 0, top: 9, height: 6, borderRadius: 3, backgroundColor: "#2c3545", ...trackStyle }} />
        <div style={{ width: `${lowPercent}%`, height: 1, flexShrink: 0 }} />
        {Array.isArray(value) && <div style={{ ...thumb, marginLeft: -8 }} />}
        <div style={{ width: `${rangePercent}%`, height: 6, flexShrink: 0, marginLeft: Array.isArray(value) ? -8 : 0, backgroundColor: "#6f7cff", borderRadius: 3, ...rangeStyle }} />
        <div style={{ ...thumb, marginLeft: -8 }} />
      </button>
    )
  },
)
