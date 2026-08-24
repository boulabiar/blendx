import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { BlendxElement, HostProps } from "../types.js"
import { FloatingLayer, normalizeKey, resolveStyle, setRefs, useControllableState } from "./floating.js"
import type { FloatingContentProps, StateStyle } from "./floating.js"

type ItemRecord = { value: string; label: React.ReactNode; disabled: boolean }
type SelectContextValue = {
  open: boolean
  value?: string
  active?: string
  anchorId?: number
  items: ItemRecord[]
  setAnchor: (element: BlendxElement | null) => void
  setOpen: (open: boolean) => void
  move: (delta: number) => void
  choose: (value: string) => void
  setActive: (value: string) => void
  ignoreNextClick: React.MutableRefObject<boolean>
}

const SelectContext = React.createContext<SelectContextValue | null>(null)
function useSelect(name: string) {
  const value = React.useContext(SelectContext)
  if (!value) throw new Error(`${name} must be used inside Select`)
  return value
}

function textContent(node: React.ReactNode): string {
  if (typeof node === "string" || typeof node === "number") return String(node)
  if (!React.isValidElement<{ children?: React.ReactNode }>(node)) return ""
  return React.Children.toArray(node.props.children).map(textContent).join("")
}

function collectItems(node: React.ReactNode, result: ItemRecord[] = []): ItemRecord[] {
  for (const child of React.Children.toArray(node)) {
    if (React.isValidElement<SelectItemProps>(child) && child.type === SelectItem) {
      result.push({ value: child.props.value, label: child.props.children, disabled: child.props.disabled ?? false })
    } else if (React.isValidElement<{ children?: React.ReactNode }>(child)) {
      collectItems(child.props.children, result)
    }
  }
  return result
}

export interface SelectProps {
  children?: React.ReactNode
  value?: string
  defaultValue?: string
  onValueChange?: (value: string) => void
  open?: boolean
  defaultOpen?: boolean
  onOpenChange?: (open: boolean) => void
  disabled?: boolean
}

export function Select({ children, value: valueProp, defaultValue, onValueChange, open: openProp, defaultOpen = false, onOpenChange, disabled = false }: SelectProps) {
  const [value, setValue] = useControllableState<string | undefined>({ value: valueProp, defaultValue, onChange: (next) => { if (next !== undefined) onValueChange?.(next) } })
  const [open, setOpenState] = useControllableState({ value: openProp, defaultValue: defaultOpen, onChange: onOpenChange })
  const [active, setActive] = React.useState<string>()
  const [anchorId, setAnchorId] = React.useState<number>()
  const triggerRef = React.useRef<BlendxElement | null>(null)
  const ignoreNextClick = React.useRef(false)
  const items = React.useMemo(() => collectItems(children), [children])
  const setOpen = (next: boolean) => {
    if (disabled) return
    setOpenState(next)
    if (next) setActive(items.find((item) => item.value === value && !item.disabled)?.value ?? items.find((item) => !item.disabled)?.value)
    else if (triggerRef.current) loadNativeRenderer().focusElement(triggerRef.current.id)
  }
  const move = (delta: number) => {
    const enabled = items.filter((item) => !item.disabled)
    if (!enabled.length) return
    const index = enabled.findIndex((item) => item.value === active)
    setActive(enabled[(index + delta + enabled.length) % enabled.length]?.value ?? enabled[0]!.value)
  }
  const choose = (next: string) => {
    if (items.find((item) => item.value === next)?.disabled) return
    setValue(next)
    setOpen(false)
  }
  const context = React.useMemo(() => ({ open, value, active, anchorId, items, setAnchor: (element: BlendxElement | null) => {
    triggerRef.current = element
    setAnchorId(element?.id)
  }, setOpen, move, choose, setActive, ignoreNextClick }), [open, value, active, anchorId, items, disabled])
  return <SelectContext.Provider value={context}>{children}</SelectContext.Provider>
}

export interface SelectTriggerProps extends HostProps {}
export const SelectTrigger = React.forwardRef<BlendxElement, SelectTriggerProps>(
  function SelectTrigger({ onMouseDown, onClick, onKeyDown, ...props }, ref) {
    const select = useSelect("SelectTrigger")
    return (
      <button
        {...props}
        ref={(element) => { select.setAnchor(element); setRefs(element, ref) }}
        tabIndex={props.tabIndex ?? 0}
        onMouseDown={(event) => {
          onMouseDown?.(event)
          if (select.open) select.ignoreNextClick.current = true
        }}
        onClick={(event) => {
          onClick?.(event)
          if (select.ignoreNextClick.current) { select.ignoreNextClick.current = false; select.setOpen(false); return }
          select.setOpen(!select.open)
        }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "escape") select.setOpen(false)
          else if (key === "down") { if (!select.open) select.setOpen(true); select.move(1) }
          else if (key === "up") { if (!select.open) select.setOpen(true); select.move(-1) }
        }}
      />
    )
  },
)

export interface SelectValueProps extends Omit<HostProps, "placeholder"> { placeholder?: React.ReactNode }
export const SelectValue = React.forwardRef<BlendxElement, SelectValueProps>(
  function SelectValue({ placeholder, children, ...props }, ref) {
    const select = useSelect("SelectValue")
    const item = select.items.find((candidate) => candidate.value === select.value)
    return <div {...props} ref={ref}>{children ?? item?.label ?? placeholder}</div>
  },
)

export interface SelectContentProps extends FloatingContentProps {}
export const SelectContent = React.forwardRef<BlendxElement, SelectContentProps>(
  function SelectContent({ onMouseDownOutside, onKeyDown, ...props }, ref) {
    const select = useSelect("SelectContent")
    if (!select.open || select.anchorId === undefined) return null
    return (
      <FloatingLayer
        {...props}
        ref={ref}
        anchorId={select.anchorId}
        tabIndex={0}
        autoFocus
        onMouseDownOutside={(event) => { onMouseDownOutside?.(event); select.setOpen(false) }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "escape") select.setOpen(false)
          else if (key === "down") select.move(1)
          else if (key === "up") select.move(-1)
          else if ((key === "enter" || key === "space") && select.active) select.choose(select.active)
        }}
      />
    )
  },
)

export interface SelectItemState { selected: boolean; highlighted: boolean; disabled: boolean }
export interface SelectItemProps extends Omit<HostProps, "style"> {
  value: string
  disabled?: boolean
  style?: StateStyle<SelectItemState>
}
export const SelectItem = React.forwardRef<BlendxElement, SelectItemProps>(
  function SelectItem({ value, disabled = false, style, onMouseEnter, onClick, ...props }, ref) {
    const select = useSelect("SelectItem")
    const state = { selected: select.value === value, highlighted: select.active === value, disabled }
    return (
      <button
        {...props}
        ref={ref}
        disabled={disabled}
        tabIndex={-1}
        style={resolveStyle(style, state)}
        onMouseEnter={(event) => { onMouseEnter?.(event); if (!disabled) select.setActive(value) }}
        onClick={(event) => { onClick?.(event); if (!disabled) select.choose(value) }}
      />
    )
  },
)

export const SelectGroup = React.forwardRef<BlendxElement, HostProps>((props, ref) => <div {...props} ref={ref} />)
export const SelectLabel = React.forwardRef<BlendxElement, HostProps>((props, ref) => <div {...props} ref={ref} />)
export const SelectSeparator = React.forwardRef<BlendxElement, HostProps>((props, ref) => <separator {...props} ref={ref} />)

export const _selectTextContent = textContent
