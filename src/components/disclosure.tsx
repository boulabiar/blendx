import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { BlendxElement, HostProps } from "../types.js"
import { normalizeKey, resolveStyle, setRefs, useControllableState } from "./floating.js"
import type { StateStyle } from "./floating.js"

export interface CollapsibleState { open: boolean; disabled: boolean; focused: boolean }
type CollapsibleContextValue = { open: boolean; disabled: boolean; setOpen: (open: boolean) => void }
const CollapsibleContext = React.createContext<CollapsibleContextValue | null>(null)
function useCollapsible(name: string): CollapsibleContextValue {
  const context = React.useContext(CollapsibleContext)
  if (!context) throw new Error(`${name} must be used inside Collapsible`)
  return context
}

export interface CollapsibleProps extends Omit<HostProps, "onChange"> {
  open?: boolean
  defaultOpen?: boolean
  onOpenChange?: (open: boolean) => void
  disabled?: boolean
}

export const Collapsible = React.forwardRef<BlendxElement, CollapsibleProps>(
  function Collapsible({ open: openProp, defaultOpen = false, onOpenChange, disabled = false, ...props }, ref) {
    const [open, setOpenState] = useControllableState({ value: openProp, defaultValue: defaultOpen, onChange: onOpenChange })
    const setOpen = React.useCallback((next: boolean) => { if (!disabled) setOpenState(next) }, [disabled, setOpenState])
    const context = React.useMemo(() => ({ open, disabled, setOpen }), [open, disabled, setOpen])
    return <CollapsibleContext.Provider value={context}><div {...props} ref={ref} /></CollapsibleContext.Provider>
  },
)

export interface CollapsibleTriggerProps extends Omit<HostProps, "style"> { style?: StateStyle<CollapsibleState> }
export const CollapsibleTrigger = React.forwardRef<BlendxElement, CollapsibleTriggerProps>(
  function CollapsibleTrigger({ style, onClick, onFocus, onBlur, ...props }, ref) {
    const disclosure = useCollapsible("CollapsibleTrigger")
    const [focused, setFocused] = React.useState(false)
    const state = { open: disclosure.open, disabled: disclosure.disabled, focused }
    return (
      <button
        {...props}
        ref={ref}
        disabled={disclosure.disabled}
        tabIndex={props.tabIndex ?? 0}
        style={resolveStyle(style, state)}
        onClick={(event) => { if (disclosure.disabled) return; onClick?.(event); disclosure.setOpen(!disclosure.open) }}
        onFocus={(event) => { onFocus?.(event); setFocused(true) }}
        onBlur={(event) => { onBlur?.(event); setFocused(false) }}
      />
    )
  },
)

export interface CollapsibleContentProps extends HostProps { forceMount?: boolean }
export const CollapsibleContent = React.forwardRef<BlendxElement, CollapsibleContentProps>(
  function CollapsibleContent({ forceMount = false, style, ...props }, ref) {
    const disclosure = useCollapsible("CollapsibleContent")
    if (!forceMount && !disclosure.open) return null
    return <div {...props} ref={ref} style={{ ...style, visibility: disclosure.open ? "visible" : "hidden" }} />
  },
)

type AccordionValue = string | string[]
type AccordionTriggerRecord = { value: string; disabled: boolean; element: React.MutableRefObject<BlendxElement | null> }
type AccordionContextValue = {
  type: "single" | "multiple"
  value: AccordionValue
  collapsible: boolean
  disabled: boolean
  toggle: (value: string) => void
  register: (trigger: AccordionTriggerRecord) => () => void
  move: (from: string, direction: number | "first" | "last") => void
}
const AccordionContext = React.createContext<AccordionContextValue | null>(null)
const AccordionItemContext = React.createContext<{ value: string; open: boolean; disabled: boolean } | null>(null)

export interface AccordionProps extends Omit<HostProps, "value" | "onChange"> {
  type?: "single" | "multiple"
  value?: AccordionValue
  defaultValue?: AccordionValue
  onValueChange?: (value: AccordionValue) => void
  collapsible?: boolean
  disabled?: boolean
}

export const Accordion = React.forwardRef<BlendxElement, AccordionProps>(
  function Accordion({ type = "single", value: valueProp, defaultValue, onValueChange, collapsible = false, disabled = false, ...props }, ref) {
    const initial = defaultValue ?? (type === "multiple" ? [] : "")
    const [value, setValue] = useControllableState<AccordionValue>({ value: valueProp, defaultValue: initial, onChange: onValueChange })
    const triggers = React.useRef<AccordionTriggerRecord[]>([])
    const register = React.useCallback((trigger: AccordionTriggerRecord) => {
      triggers.current.push(trigger)
      return () => { triggers.current = triggers.current.filter((candidate) => candidate !== trigger) }
    }, [])
    const toggle = React.useCallback((next: string) => {
      if (disabled) return
      if (type === "multiple") {
        const selected = Array.isArray(value) ? value : []
        setValue(selected.includes(next) ? selected.filter((item) => item !== next) : [...selected, next])
      } else {
        const current = Array.isArray(value) ? value[0] ?? "" : value
        if (current === next && collapsible) setValue("")
        else if (current !== next) setValue(next)
      }
    }, [disabled, type, value, collapsible, setValue])
    const move = React.useCallback((from: string, direction: number | "first" | "last") => {
      const enabled = triggers.current.filter((trigger) => !trigger.disabled)
      if (!enabled.length) return
      const current = enabled.findIndex((trigger) => trigger.value === from)
      const index = direction === "first" ? 0 : direction === "last" ? enabled.length - 1 : (Math.max(0, current) + direction + enabled.length) % enabled.length
      const next = enabled[index] ?? enabled[0]!
      if (next.element.current) loadNativeRenderer().focusElement(next.element.current.id)
    }, [])
    const context = React.useMemo(() => ({ type, value, collapsible, disabled, toggle, register, move }), [type, value, collapsible, disabled, toggle, register, move])
    return <AccordionContext.Provider value={context}><div {...props} ref={ref} /></AccordionContext.Provider>
  },
)

export interface AccordionItemState { open: boolean; disabled: boolean }
export interface AccordionItemProps extends Omit<HostProps, "style" | "value"> { value: string; disabled?: boolean; style?: StateStyle<AccordionItemState> }
export const AccordionItem = React.forwardRef<BlendxElement, AccordionItemProps>(
  function AccordionItem({ value, disabled: itemDisabled = false, style, ...props }, ref) {
    const accordion = React.useContext(AccordionContext)
    if (!accordion) throw new Error("AccordionItem must be used inside Accordion")
    const open = Array.isArray(accordion.value) ? accordion.value.includes(value) : accordion.value === value
    const disabled = accordion.disabled || itemDisabled
    const state = { open, disabled }
    return <AccordionItemContext.Provider value={{ value, open, disabled }}><div {...props} ref={ref} style={resolveStyle(style, state)} /></AccordionItemContext.Provider>
  },
)

export interface AccordionTriggerState extends AccordionItemState { focused: boolean }
export interface AccordionTriggerProps extends Omit<HostProps, "style"> { style?: StateStyle<AccordionTriggerState> }
export const AccordionTrigger = React.forwardRef<BlendxElement, AccordionTriggerProps>(
  function AccordionTrigger({ style, onClick, onKeyDown, onFocus, onBlur, ...props }, forwardedRef) {
    const accordion = React.useContext(AccordionContext)
    const item = React.useContext(AccordionItemContext)
    if (!accordion || !item) throw new Error("AccordionTrigger must be used inside AccordionItem")
    const element = React.useRef<BlendxElement | null>(null)
    const [focused, setFocused] = React.useState(false)
    const state = { open: item.open, disabled: item.disabled, focused }
    React.useEffect(() => accordion.register({ value: item.value, disabled: item.disabled, element }), [accordion.register, item.value, item.disabled])
    return (
      <button
        {...props}
        ref={(next) => { element.current = next; setRefs(next, forwardedRef) }}
        disabled={item.disabled}
        tabIndex={props.tabIndex ?? 0}
        style={resolveStyle(style, state)}
        onClick={(event) => { if (item.disabled) return; onClick?.(event); accordion.toggle(item.value) }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "home") accordion.move(item.value, "first")
          else if (key === "end") accordion.move(item.value, "last")
          else if (key === "down") accordion.move(item.value, 1)
          else if (key === "up") accordion.move(item.value, -1)
        }}
        onFocus={(event) => { onFocus?.(event); setFocused(true) }}
        onBlur={(event) => { onBlur?.(event); setFocused(false) }}
      />
    )
  },
)

export interface AccordionContentProps extends HostProps { forceMount?: boolean }
export const AccordionContent = React.forwardRef<BlendxElement, AccordionContentProps>(
  function AccordionContent({ forceMount = false, style, ...props }, ref) {
    const item = React.useContext(AccordionItemContext)
    if (!item) throw new Error("AccordionContent must be used inside AccordionItem")
    if (!forceMount && !item.open) return null
    return <div {...props} ref={ref} style={{ ...style, visibility: item.open ? "visible" : "hidden" }} />
  },
)
