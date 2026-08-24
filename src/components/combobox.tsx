import React from "react"
import type { BlendxElement, HostProps } from "../types.js"
import { FloatingLayer, normalizeKey, resolveStyle, setRefs, useControllableState } from "./floating.js"
import type { FloatingContentProps, StateStyle } from "./floating.js"

export type ComboboxValue = string | string[] | null
type ContextValue = {
  open: boolean
  value: ComboboxValue
  input: string
  filtered: string[]
  active: number | null
  anchorId?: number
  multiple: boolean
  disabled: boolean
  setAnchor: (element: BlendxElement | null) => void
  setOpen: (open: boolean) => void
  setInput: (value: string) => void
  setActive: (index: number | null) => void
  move: (delta: number) => void
  choose: (value: string) => void
}

const Context = React.createContext<ContextValue | null>(null)
function useCombobox(name: string) {
  const value = React.useContext(Context)
  if (!value) throw new Error(`${name} must be used inside Combobox`)
  return value
}

export interface ComboboxProps {
  children?: React.ReactNode
  items: string[]
  value?: ComboboxValue
  defaultValue?: ComboboxValue
  onValueChange?: (value: ComboboxValue) => void
  inputValue?: string
  defaultInputValue?: string
  onInputValueChange?: (value: string) => void
  open?: boolean
  defaultOpen?: boolean
  onOpenChange?: (open: boolean) => void
  multiple?: boolean
  disabled?: boolean
  filter?: ((item: string, query: string) => boolean) | null
}

export function Combobox({ children, items, value: valueProp, defaultValue = null, onValueChange, inputValue, defaultInputValue = "", onInputValueChange, open: openProp, defaultOpen = false, onOpenChange, multiple = false, disabled = false, filter }: ComboboxProps) {
  const [value, setValue] = useControllableState({ value: valueProp, defaultValue, onChange: onValueChange })
  const [input, setInputState] = useControllableState({ value: inputValue, defaultValue: defaultInputValue, onChange: onInputValueChange })
  const [open, setOpenState] = useControllableState({ value: openProp, defaultValue: defaultOpen, onChange: onOpenChange })
  const [active, setActive] = React.useState<number | null>(null)
  const [anchorId, setAnchorId] = React.useState<number>()
  const filtered = React.useMemo(() => filter === null ? items : items.filter((item) => filter ? filter(item, input) : item.toLowerCase().includes(input.toLowerCase())), [items, input, filter])
  const setOpen = (next: boolean) => { if (!disabled) setOpenState(next) }
  const setInput = (next: string) => { setInputState(next); setActive(null); setOpen(true) }
  const move = (delta: number) => {
    if (!filtered.length) return
    const index = active === null ? (delta > 0 ? 0 : filtered.length - 1) : (active + delta + filtered.length) % filtered.length
    setActive(index)
  }
  const choose = (item: string) => {
    if (multiple) {
      const selected = Array.isArray(value) ? value : []
      setValue(selected.includes(item) ? selected.filter((candidate) => candidate !== item) : [...selected, item])
      setInputState("")
    } else {
      setValue(item)
      setInputState(item)
      setOpen(false)
    }
  }
  const context = React.useMemo(() => ({ open, value, input, filtered, active, anchorId, multiple, disabled, setAnchor: (element: BlendxElement | null) => setAnchorId(element?.id), setOpen, setInput, setActive, move, choose }), [open, value, input, filtered, active, anchorId, multiple, disabled])
  return <Context.Provider value={context}>{children}</Context.Provider>
}

export interface ComboboxInputProps extends HostProps {}
export const ComboboxInput = React.forwardRef<BlendxElement, ComboboxInputProps>(
  function ComboboxInput({ onChange, onFocus, onClick, onKeyDown, onSubmit, ...props }, ref) {
    const box = useCombobox("ComboboxInput")
    return (
      <input
        {...props}
        ref={(element) => { box.setAnchor(element); setRefs(element, ref) }}
        value={box.input}
        readOnly={box.disabled || props.readOnly}
        onFocus={(event) => { onFocus?.(event); box.setOpen(true) }}
        onClick={(event) => { onClick?.(event); box.setOpen(true) }}
        onChange={(event) => { onChange?.(event); box.setInput(event.value ?? "") }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "escape") box.setOpen(false)
          else if (key === "down") { box.setOpen(true); box.move(1) }
          else if (key === "up") { box.setOpen(true); box.move(-1) }
        }}
        onSubmit={(event) => {
          onSubmit?.(event)
          if (box.active !== null && box.filtered[box.active]) box.choose(box.filtered[box.active]!)
        }}
      />
    )
  },
)

export interface ComboboxContentProps extends FloatingContentProps {}
export const ComboboxContent = React.forwardRef<BlendxElement, ComboboxContentProps>(
  function ComboboxContent({ onMouseDownOutside, ...props }, ref) {
    const box = useCombobox("ComboboxContent")
    if (!box.open || box.anchorId === undefined) return null
    return <FloatingLayer {...props} ref={ref} anchorId={box.anchorId} onMouseDownOutside={(event) => { onMouseDownOutside?.(event); box.setOpen(false) }} />
  },
)

export interface ComboboxListProps extends Omit<HostProps, "children"> { children?: React.ReactNode | ((item: string) => React.ReactNode) }
export const ComboboxList = React.forwardRef<BlendxElement, ComboboxListProps>(
  function ComboboxList({ children, ...props }, ref) {
    const box = useCombobox("ComboboxList")
    return <div {...props} ref={ref}>{typeof children === "function" ? box.filtered.map(children) : children}</div>
  },
)

export interface ComboboxItemState { selected: boolean; highlighted: boolean; disabled: boolean }
export interface ComboboxItemProps extends Omit<HostProps, "style"> { value: string; disabled?: boolean; style?: StateStyle<ComboboxItemState> }
export const ComboboxItem = React.forwardRef<BlendxElement, ComboboxItemProps>(
  function ComboboxItem({ value, disabled = false, style, onMouseEnter, onClick, ...props }, ref) {
    const box = useCombobox("ComboboxItem")
    const index = box.filtered.indexOf(value)
    const selected = Array.isArray(box.value) ? box.value.includes(value) : box.value === value
    const state = { selected, highlighted: box.active === index, disabled }
    return (
      <button
        {...props}
        ref={ref}
        disabled={disabled}
        tabIndex={-1}
        style={resolveStyle(style, state)}
        onMouseEnter={(event) => { onMouseEnter?.(event); if (!disabled) box.setActive(index) }}
        onClick={(event) => { onClick?.(event); if (!disabled) box.choose(value) }}
      />
    )
  },
)

export const ComboboxEmpty = React.forwardRef<BlendxElement, HostProps>(
  function ComboboxEmpty(props, ref) {
    const box = useCombobox("ComboboxEmpty")
    return box.filtered.length ? null : <div {...props} ref={ref} />
  },
)

export interface ComboboxValueProps extends Omit<HostProps, "children" | "placeholder"> { placeholder?: React.ReactNode; children?: React.ReactNode | ((value: ComboboxValue) => React.ReactNode) }
export const ComboboxValue = React.forwardRef<BlendxElement, ComboboxValueProps>(
  function ComboboxValue({ placeholder, children, ...props }, ref) {
    const box = useCombobox("ComboboxValue")
    const display = Array.isArray(box.value) ? box.value.join(", ") : box.value
    return <div {...props} ref={ref}>{typeof children === "function" ? children(box.value) : children ?? display ?? placeholder}</div>
  },
)

export const ComboboxGroup = React.forwardRef<BlendxElement, HostProps>((props, ref) => <div {...props} ref={ref} />)
export const ComboboxLabel = React.forwardRef<BlendxElement, HostProps>((props, ref) => <div {...props} ref={ref} />)
export const ComboboxSeparator = React.forwardRef<BlendxElement, HostProps>((props, ref) => <separator {...props} ref={ref} />)
