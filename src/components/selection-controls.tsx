import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { BlendxElement, HostProps } from "../types.js"
import { normalizeKey, resolveStyle, useControllableState } from "./floating.js"
import type { StateStyle } from "./floating.js"

type ControlChildren<State> = React.ReactNode | ((state: State) => React.ReactNode)

export type CheckedState = boolean | "indeterminate"
export interface CheckboxState { checked: CheckedState; disabled: boolean; focused: boolean }
type CheckboxContextValue = CheckboxState
const CheckboxContext = React.createContext<CheckboxContextValue | null>(null)

export interface CheckboxProps extends Omit<HostProps, "children" | "style"> {
  checked?: CheckedState
  defaultChecked?: CheckedState
  onCheckedChange?: (checked: CheckedState) => void
  children?: ControlChildren<CheckboxState>
  style?: StateStyle<CheckboxState>
}

export const Checkbox = React.forwardRef<BlendxElement, CheckboxProps>(
  function Checkbox({ checked: checkedProp, defaultChecked = false, onCheckedChange, disabled = false, children, style, onClick, onFocus, onBlur, ...props }, ref) {
    const [checked, setChecked] = useControllableState({ value: checkedProp, defaultValue: defaultChecked, onChange: onCheckedChange })
    const [focused, setFocused] = React.useState(false)
    const state = { checked, disabled, focused }
    return (
      <CheckboxContext.Provider value={state}>
        <button
          {...props}
          ref={ref}
          accessibilityRole="checkbox"
          accessibilityChecked={checked === "indeterminate" ? "mixed" : checked}
          disabled={disabled}
          tabIndex={props.tabIndex ?? 0}
          style={resolveStyle(style, state)}
          onClick={(event) => { if (disabled) return; onClick?.(event); setChecked(checked === true ? false : true) }}
          onFocus={(event) => { onFocus?.(event); setFocused(true) }}
          onBlur={(event) => { onBlur?.(event); setFocused(false) }}
        >
          {typeof children === "function" ? children(state) : children}
        </button>
      </CheckboxContext.Provider>
    )
  },
)

export interface CheckboxIndicatorProps extends HostProps { forceMount?: boolean }
export const CheckboxIndicator = React.forwardRef<BlendxElement, CheckboxIndicatorProps>(
  function CheckboxIndicator({ forceMount = false, ...props }, ref) {
    const state = React.useContext(CheckboxContext)
    if (!state) throw new Error("CheckboxIndicator must be used inside Checkbox")
    if (!forceMount && state.checked === false) return null
    return <div {...props} ref={ref} />
  },
)

export interface SwitchState { checked: boolean; disabled: boolean; focused: boolean }
const SwitchContext = React.createContext<SwitchState | null>(null)

export interface SwitchProps extends Omit<HostProps, "children" | "style"> {
  checked?: boolean
  defaultChecked?: boolean
  onCheckedChange?: (checked: boolean) => void
  children?: ControlChildren<SwitchState>
  style?: StateStyle<SwitchState>
}

export const Switch = React.forwardRef<BlendxElement, SwitchProps>(
  function Switch({ checked: checkedProp, defaultChecked = false, onCheckedChange, disabled = false, children, style, onClick, onFocus, onBlur, ...props }, ref) {
    const [checked, setChecked] = useControllableState({ value: checkedProp, defaultValue: defaultChecked, onChange: onCheckedChange })
    const [focused, setFocused] = React.useState(false)
    const state = { checked, disabled, focused }
    return (
      <SwitchContext.Provider value={state}>
        <button
          {...props}
          ref={ref}
          accessibilityRole="switch"
          accessibilityChecked={checked}
          disabled={disabled}
          tabIndex={props.tabIndex ?? 0}
          style={resolveStyle(style, state)}
          onClick={(event) => { if (disabled) return; onClick?.(event); setChecked(!checked) }}
          onFocus={(event) => { onFocus?.(event); setFocused(true) }}
          onBlur={(event) => { onBlur?.(event); setFocused(false) }}
        >
          {typeof children === "function" ? children(state) : children}
        </button>
      </SwitchContext.Provider>
    )
  },
)

export interface SwitchThumbProps extends Omit<HostProps, "style"> { style?: StateStyle<SwitchState> }
export const SwitchThumb = React.forwardRef<BlendxElement, SwitchThumbProps>(
  function SwitchThumb({ style, ...props }, ref) {
    const state = React.useContext(SwitchContext)
    if (!state) throw new Error("SwitchThumb must be used inside Switch")
    return <div {...props} ref={ref} style={resolveStyle(style, state)} />
  },
)

type RadioRegistration = { value: string; disabled: boolean; element: React.MutableRefObject<BlendxElement | null> }
type RadioContextValue = {
  value?: string
  disabled: boolean
  setValue: (value: string) => void
  register: (item: RadioRegistration) => () => void
  move: (value: string, delta: number) => void
}
const RadioContext = React.createContext<RadioContextValue | null>(null)

export interface RadioGroupProps extends Omit<HostProps, "onChange"> {
  value?: string
  defaultValue?: string
  onValueChange?: (value: string) => void
  disabled?: boolean
  orientation?: "horizontal" | "vertical"
}

export const RadioGroup = React.forwardRef<BlendxElement, RadioGroupProps>(
  function RadioGroup({ value: valueProp, defaultValue, onValueChange, disabled = false, orientation = "vertical", style, ...props }, ref) {
    const [value, setValueState] = useControllableState<string | undefined>({ value: valueProp, defaultValue, onChange: (next) => { if (next !== undefined) onValueChange?.(next) } })
    const items = React.useRef<RadioRegistration[]>([])
    const register = React.useCallback((item: RadioRegistration) => {
      items.current.push(item)
      return () => { items.current = items.current.filter((candidate) => candidate !== item) }
    }, [])
    const setValue = React.useCallback((next: string) => { if (!disabled) setValueState(next) }, [disabled, setValueState])
    const move = React.useCallback((from: string, delta: number) => {
      const enabled = items.current.filter((item) => !item.disabled)
      if (!enabled.length) return
      const index = enabled.findIndex((item) => item.value === from)
      const next = enabled[(index + delta + enabled.length) % enabled.length] ?? enabled[0]!
      setValue(next.value)
      if (next.element.current) loadNativeRenderer().focusElement(next.element.current.id)
    }, [setValue])
    const context = React.useMemo(() => ({ value, disabled, setValue, register, move }), [value, disabled, setValue, register, move])
    return (
      <RadioContext.Provider value={context}>
        <div {...props} ref={ref} style={{ flexDirection: orientation === "horizontal" ? "row" : "column", ...style }} />
      </RadioContext.Provider>
    )
  },
)

export interface RadioGroupItemState { checked: boolean; disabled: boolean; focused: boolean }
const RadioItemContext = React.createContext<RadioGroupItemState | null>(null)
export interface RadioGroupItemProps extends Omit<HostProps, "style"> {
  value: string
  disabled?: boolean
  style?: StateStyle<RadioGroupItemState>
}

export const RadioGroupItem = React.forwardRef<BlendxElement, RadioGroupItemProps>(
  function RadioGroupItem({ value, disabled: itemDisabled = false, style, onClick, onKeyDown, onFocus, onBlur, ...props }, forwardedRef) {
    const group = React.useContext(RadioContext)
    if (!group) throw new Error("RadioGroupItem must be used inside RadioGroup")
    const element = React.useRef<BlendxElement | null>(null)
    const [focused, setFocused] = React.useState(false)
    const disabled = group.disabled || itemDisabled
    const state = { checked: group.value === value, disabled, focused }
    React.useEffect(() => group.register({ value, disabled, element }), [group.register, value, disabled])
    const setElement = (next: BlendxElement | null) => {
      element.current = next
      if (typeof forwardedRef === "function") forwardedRef(next)
      else if (forwardedRef) (forwardedRef as React.MutableRefObject<BlendxElement | null>).current = next
    }
    return (
      <RadioItemContext.Provider value={state}>
        <button
          {...props}
          ref={setElement}
          accessibilityRole="radio"
          accessibilityChecked={state.checked}
          disabled={disabled}
          tabIndex={props.tabIndex ?? (state.checked ? 0 : -1)}
          style={resolveStyle(style, state)}
          onClick={(event) => {
            if (disabled) return
            onClick?.(event)
            group.setValue(value)
            if (element.current) loadNativeRenderer().focusElement(element.current.id)
          }}
          onKeyDown={(event) => {
            onKeyDown?.(event)
            const key = normalizeKey(event.key)
            if (key === "right" || key === "down") group.move(value, 1)
            else if (key === "left" || key === "up") group.move(value, -1)
          }}
          onFocus={(event) => { onFocus?.(event); setFocused(true) }}
          onBlur={(event) => { onBlur?.(event); setFocused(false) }}
        />
      </RadioItemContext.Provider>
    )
  },
)

export interface RadioGroupIndicatorProps extends HostProps { forceMount?: boolean }
export const RadioGroupIndicator = React.forwardRef<BlendxElement, RadioGroupIndicatorProps>(
  function RadioGroupIndicator({ forceMount = false, ...props }, ref) {
    const state = React.useContext(RadioItemContext)
    if (!state) throw new Error("RadioGroupIndicator must be used inside RadioGroupItem")
    if (!forceMount && !state.checked) return null
    return <div {...props} ref={ref} />
  },
)
