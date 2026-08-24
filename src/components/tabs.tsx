import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { BlendxElement, HostProps } from "../types.js"
import { normalizeKey, resolveStyle, setRefs, useControllableState } from "./floating.js"
import type { StateStyle } from "./floating.js"

type TriggerRecord = { value: string; disabled: boolean; element: React.MutableRefObject<BlendxElement | null> }
type TabsContextValue = {
  value?: string
  orientation: "horizontal" | "vertical"
  activationMode: "automatic" | "manual"
  setValue: (value: string) => void
  register: (trigger: TriggerRecord) => () => void
  move: (from: string, direction: number | "first" | "last") => void
}

const TabsContext = React.createContext<TabsContextValue | null>(null)
function useTabs(name: string): TabsContextValue {
  const context = React.useContext(TabsContext)
  if (!context) throw new Error(`${name} must be used inside Tabs`)
  return context
}

export interface TabsProps extends Omit<HostProps, "value" | "onChange"> {
  value?: string
  defaultValue?: string
  onValueChange?: (value: string) => void
  orientation?: "horizontal" | "vertical"
  activationMode?: "automatic" | "manual"
}

export const Tabs = React.forwardRef<BlendxElement, TabsProps>(
  function Tabs({ value: valueProp, defaultValue, onValueChange, orientation = "horizontal", activationMode = "automatic", ...props }, ref) {
    const [value, setValueState] = useControllableState<string | undefined>({ value: valueProp, defaultValue, onChange: (next) => { if (next !== undefined) onValueChange?.(next) } })
    const triggers = React.useRef<TriggerRecord[]>([])
    const register = React.useCallback((trigger: TriggerRecord) => {
      triggers.current.push(trigger)
      return () => { triggers.current = triggers.current.filter((candidate) => candidate !== trigger) }
    }, [])
    const setValue = React.useCallback((next: string) => setValueState(next), [setValueState])
    const move = React.useCallback((from: string, direction: number | "first" | "last") => {
      const enabled = triggers.current.filter((trigger) => !trigger.disabled)
      if (!enabled.length) return
      const current = enabled.findIndex((trigger) => trigger.value === from)
      const index = direction === "first" ? 0 : direction === "last" ? enabled.length - 1 : (Math.max(0, current) + direction + enabled.length) % enabled.length
      const next = enabled[index] ?? enabled[0]!
      if (activationMode === "automatic") setValue(next.value)
      if (next.element.current) loadNativeRenderer().focusElement(next.element.current.id)
    }, [activationMode, setValue])
    const context = React.useMemo(() => ({ value, orientation, activationMode, setValue, register, move }), [value, orientation, activationMode, setValue, register, move])
    return <TabsContext.Provider value={context}><div {...props} ref={ref} /></TabsContext.Provider>
  },
)

export const TabsList = React.forwardRef<BlendxElement, HostProps>((props, ref) => <div {...props} ref={ref} />)

export interface TabsTriggerState { selected: boolean; disabled: boolean; focused: boolean }
export interface TabsTriggerProps extends Omit<HostProps, "style" | "value"> {
  value: string
  disabled?: boolean
  style?: StateStyle<TabsTriggerState>
}

export const TabsTrigger = React.forwardRef<BlendxElement, TabsTriggerProps>(
  function TabsTrigger({ value, disabled = false, style, onClick, onKeyDown, onFocus, onBlur, ...props }, forwardedRef) {
    const tabs = useTabs("TabsTrigger")
    const element = React.useRef<BlendxElement | null>(null)
    const [focused, setFocused] = React.useState(false)
    const state = { selected: tabs.value === value, disabled, focused }
    React.useEffect(() => tabs.register({ value, disabled, element }), [tabs.register, value, disabled])
    return (
      <button
        {...props}
        ref={(next) => { element.current = next; setRefs(next, forwardedRef) }}
        accessibilityRole="tab"
        accessibilitySelected={state.selected}
        disabled={disabled}
        tabIndex={props.tabIndex ?? (state.selected ? 0 : -1)}
        style={resolveStyle(style, state)}
        onClick={(event) => { if (disabled) return; onClick?.(event); tabs.setValue(value); if (element.current) loadNativeRenderer().focusElement(element.current.id) }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "home") tabs.move(value, "first")
          else if (key === "end") tabs.move(value, "last")
          else if ((tabs.orientation === "horizontal" && key === "right") || (tabs.orientation === "vertical" && key === "down")) tabs.move(value, 1)
          else if ((tabs.orientation === "horizontal" && key === "left") || (tabs.orientation === "vertical" && key === "up")) tabs.move(value, -1)
        }}
        onFocus={(event) => { onFocus?.(event); setFocused(true); if (!disabled && tabs.activationMode === "automatic") tabs.setValue(value) }}
        onBlur={(event) => { onBlur?.(event); setFocused(false) }}
      />
    )
  },
)

export interface TabsContentProps extends HostProps { value: string; forceMount?: boolean }
export const TabsContent = React.forwardRef<BlendxElement, TabsContentProps>(
  function TabsContent({ value, forceMount = false, style, ...props }, ref) {
    const tabs = useTabs("TabsContent")
    const selected = tabs.value === value
    if (!forceMount && !selected) return null
    return <div {...props} ref={ref} style={{ ...style, visibility: selected ? "visible" : "hidden" }} />
  },
)
