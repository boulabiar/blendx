import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { AnchorPosition, BlendxElement, HostProps } from "../types.js"
import { FloatingLayer, normalizeKey, resolveStyle, setRefs, useControllableState } from "./floating.js"
import type { FloatingContentProps, StateStyle } from "./floating.js"

type MenuEntry = {
  value: string
  label: string
  disabled: boolean
  action: React.MutableRefObject<() => void>
  submenu?: boolean
}
type MenuContextValue = {
  open: boolean
  disabled: boolean
  active?: string
  anchorId?: number
  point?: AnchorPosition
  ignoreNextClick: React.MutableRefObject<boolean>
  setTrigger: (element: BlendxElement | null) => void
  setOpen: (open: boolean) => void
  openAt: (position: AnchorPosition) => void
  setActive: (value?: string) => void
  register: (entry: MenuEntry) => () => void
  move: (direction: number | "first" | "last") => void
  activate: (value: string, submenuOnly?: boolean) => void
  typeahead: (key: string) => void
}

const MenuContext = React.createContext<MenuContextValue | null>(null)
function useMenu(name: string): MenuContextValue {
  const context = React.useContext(MenuContext)
  if (!context) throw new Error(`${name} must be used inside a menu root`)
  return context
}

function useMenuState({ open: openProp, defaultOpen, onOpenChange, disabled }: { open?: boolean; defaultOpen: boolean; onOpenChange?: (open: boolean) => void; disabled: boolean }): MenuContextValue {
  const [open, setOpenState] = useControllableState({ value: openProp, defaultValue: defaultOpen, onChange: onOpenChange })
  const [active, setActive] = React.useState<string>()
  const [anchorId, setAnchorId] = React.useState<number>()
  const [point, setPoint] = React.useState<AnchorPosition>()
  const trigger = React.useRef<BlendxElement | null>(null)
  const entries = React.useRef<MenuEntry[]>([])
  const ignoreNextClick = React.useRef(false)
  const setTrigger = React.useCallback((element: BlendxElement | null) => { trigger.current = element; setAnchorId(element?.id) }, [])
  const setOpen = React.useCallback((next: boolean) => {
    if (disabled && next) return
    setOpenState(next)
    if (!next) {
      setActive(undefined)
      if (trigger.current) loadNativeRenderer().focusElement(trigger.current.id)
    }
  }, [disabled, setOpenState])
  const openAt = React.useCallback((position: AnchorPosition) => { if (!disabled) { setPoint(position); setOpenState(true) } }, [disabled, setOpenState])
  const register = React.useCallback((entry: MenuEntry) => {
    entries.current.push(entry)
    return () => { entries.current = entries.current.filter((candidate) => candidate !== entry) }
  }, [])
  const move = React.useCallback((direction: number | "first" | "last") => {
    const enabled = entries.current.filter((entry) => !entry.disabled)
    if (!enabled.length) return
    const current = enabled.findIndex((entry) => entry.value === active)
    const index = direction === "first" ? 0 : direction === "last" ? enabled.length - 1 : (current + direction + enabled.length) % enabled.length
    setActive((enabled[index] ?? enabled[0])?.value)
  }, [active])
  const activate = React.useCallback((value: string, submenuOnly = false) => {
    const entry = entries.current.find((candidate) => candidate.value === value && !candidate.disabled)
    if (entry && (!submenuOnly || entry.submenu)) entry.action.current()
  }, [])
  const typeahead = React.useCallback((key: string) => {
    if (key.length !== 1) return
    const match = entries.current.find((entry) => !entry.disabled && entry.label.toLowerCase().startsWith(key.toLowerCase()))
    if (match) setActive(match.value)
  }, [])
  return React.useMemo(() => ({ open, disabled, active, anchorId, point, ignoreNextClick, setTrigger, setOpen, openAt, setActive, register, move, activate, typeahead }), [open, disabled, active, anchorId, point, setTrigger, setOpen, openAt, register, move, activate, typeahead])
}

export interface MenuRootProps {
  children?: React.ReactNode
  open?: boolean
  defaultOpen?: boolean
  onOpenChange?: (open: boolean) => void
  disabled?: boolean
}

export function DropdownMenu({ children, open, defaultOpen = false, onOpenChange, disabled = false }: MenuRootProps) {
  const menu = useMenuState({ open, defaultOpen, onOpenChange, disabled })
  return <MenuContext.Provider value={menu}>{children}</MenuContext.Provider>
}

export interface DropdownMenuTriggerProps extends HostProps {}
export const DropdownMenuTrigger = React.forwardRef<BlendxElement, DropdownMenuTriggerProps>(
  function DropdownMenuTrigger({ onMouseDown, onClick, onKeyDown, ...props }, ref) {
    const menu = useMenu("DropdownMenuTrigger")
    return (
      <button
        {...props}
        ref={(element) => { menu.setTrigger(element); setRefs(element, ref) }}
        disabled={menu.disabled}
        tabIndex={props.tabIndex ?? 0}
        onMouseDown={(event) => { onMouseDown?.(event); if (menu.open) menu.ignoreNextClick.current = true }}
        onClick={(event) => {
          onClick?.(event)
          if (menu.ignoreNextClick.current) { menu.ignoreNextClick.current = false; menu.setOpen(false); return }
          menu.setOpen(!menu.open)
        }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "down" || key === "up") menu.setOpen(true)
          else if (key === "escape") menu.setOpen(false)
        }}
      />
    )
  },
)

export interface DropdownMenuContentProps extends FloatingContentProps {}
export const DropdownMenuContent = React.forwardRef<BlendxElement, DropdownMenuContentProps>(
  function DropdownMenuContent(props, ref) {
    const menu = useMenu("DropdownMenuContent")
    if (!menu.open || menu.anchorId === undefined) return null
    return <MenuContent {...props} ref={ref} anchorId={menu.anchorId} />
  },
)

export function ContextMenu({ children, open, defaultOpen = false, onOpenChange, disabled = false }: MenuRootProps) {
  const menu = useMenuState({ open, defaultOpen, onOpenChange, disabled })
  return <MenuContext.Provider value={menu}>{children}</MenuContext.Provider>
}

export interface ContextMenuTriggerProps extends HostProps {}
export const ContextMenuTrigger = React.forwardRef<BlendxElement, ContextMenuTriggerProps>(
  function ContextMenuTrigger({ onMouseDown, onKeyDown, ...props }, ref) {
    const menu = useMenu("ContextMenuTrigger")
    const element = React.useRef<BlendxElement | null>(null)
    return (
      <div
        {...props}
        ref={(next) => { element.current = next; menu.setTrigger(next); setRefs(next, ref) }}
        tabIndex={props.tabIndex ?? 0}
        onMouseDown={(event) => {
          onMouseDown?.(event)
          if (event.button === 3) menu.openAt({ x: event.x, y: event.y })
        }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          if (normalizeKey(event.key) === "f10" && element.current) {
            const box = loadNativeRenderer().getElementBox(element.current.id)
            menu.openAt({ x: box.x + 12, y: box.y + 12 })
          }
        }}
      />
    )
  },
)

export interface ContextMenuContentProps extends FloatingContentProps {}
export const ContextMenuContent = React.forwardRef<BlendxElement, ContextMenuContentProps>(
  function ContextMenuContent(props, ref) {
    const menu = useMenu("ContextMenuContent")
    if (!menu.open || !menu.point) return null
    return <MenuContent {...props} ref={ref} position={menu.point} side="bottom" align="start" sideOffset={0} />
  },
)

const MenuContent = React.forwardRef<BlendxElement, FloatingContentProps>(
  function MenuContent({ onMouseDownOutside, onKeyDown, ...props }, ref) {
    const menu = useMenu("MenuContent")
    React.useLayoutEffect(() => menu.move("first"), [])
    return (
      <FloatingLayer
        {...props}
        ref={ref}
        accessibilityRole="menu"
        tabIndex={0}
        autoFocus
        onMouseDownOutside={(event) => { onMouseDownOutside?.(event); menu.setOpen(false) }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "escape") menu.setOpen(false)
          else if (key === "down") menu.move(1)
          else if (key === "up") menu.move(-1)
          else if (key === "home") menu.move("first")
          else if (key === "end") menu.move("last")
          else if (key === "right" && menu.active) menu.activate(menu.active, true)
          else if ((key === "enter" || key === "space") && menu.active) menu.activate(menu.active)
          else menu.typeahead(key)
        }}
      />
    )
  },
)

export interface MenuItemState { highlighted: boolean; disabled: boolean }
export interface MenuItemProps extends Omit<HostProps, "style" | "value"> {
  value: string
  label?: string
  disabled?: boolean
  closeOnSelect?: boolean
  onSelect?: () => void
  style?: StateStyle<MenuItemState>
}

const MenuItem = React.forwardRef<BlendxElement, MenuItemProps>(
  function MenuItem({ value, label = value, disabled = false, closeOnSelect = true, onSelect, style, onMouseEnter, onClick, ...props }, ref) {
    const menu = useMenu("MenuItem")
    const action = React.useRef(() => {})
    action.current = () => { if (!disabled) { onSelect?.(); if (closeOnSelect) menu.setOpen(false) } }
    React.useLayoutEffect(() => menu.register({ value, label, disabled, action }), [menu.register, value, label, disabled])
    const state = { highlighted: menu.active === value, disabled }
    return (
      <button
        {...props}
        ref={ref}
        accessibilityRole="menuitem"
        disabled={disabled}
        tabIndex={-1}
        style={resolveStyle(style, state)}
        onMouseEnter={(event) => { onMouseEnter?.(event); if (!disabled) menu.setActive(value) }}
        onClick={(event) => { if (disabled) return; onClick?.(event); action.current() }}
      />
    )
  },
)

export const DropdownMenuItem = MenuItem
export const ContextMenuItem = MenuItem

export interface MenuCheckboxItemState extends MenuItemState { checked: boolean }
export interface MenuCheckboxItemProps extends Omit<MenuItemProps, "style" | "onSelect"> {
  checked?: boolean
  defaultChecked?: boolean
  onCheckedChange?: (checked: boolean) => void
  style?: StateStyle<MenuCheckboxItemState>
}

const MenuCheckboxItem = React.forwardRef<BlendxElement, MenuCheckboxItemProps>(
  function MenuCheckboxItem({ checked: checkedProp, defaultChecked = false, onCheckedChange, style, closeOnSelect = false, ...props }, ref) {
    const [checked, setChecked] = useControllableState({ value: checkedProp, defaultValue: defaultChecked, onChange: onCheckedChange })
    const menu = useMenu("MenuCheckboxItem")
    const state = { highlighted: menu.active === props.value, disabled: props.disabled ?? false, checked }
    return <MenuItem {...props} ref={ref} closeOnSelect={closeOnSelect} style={resolveStyle(style, state)} onSelect={() => setChecked(!checked)} />
  },
)

export const DropdownMenuCheckboxItem = MenuCheckboxItem
export const ContextMenuCheckboxItem = MenuCheckboxItem

type MenuRadioContextValue = { value?: string; setValue: (value: string) => void }
const MenuRadioContext = React.createContext<MenuRadioContextValue | null>(null)
export interface MenuRadioGroupProps extends Omit<HostProps, "value" | "onChange"> { value?: string; defaultValue?: string; onValueChange?: (value: string) => void }
const MenuRadioGroup = React.forwardRef<BlendxElement, MenuRadioGroupProps>(
  function MenuRadioGroup({ value: valueProp, defaultValue, onValueChange, ...props }, ref) {
    const [value, setValueState] = useControllableState<string | undefined>({ value: valueProp, defaultValue, onChange: (next) => { if (next !== undefined) onValueChange?.(next) } })
    return <MenuRadioContext.Provider value={{ value, setValue: setValueState }}><div {...props} ref={ref} /></MenuRadioContext.Provider>
  },
)
export const DropdownMenuRadioGroup = MenuRadioGroup
export const ContextMenuRadioGroup = MenuRadioGroup

export interface MenuRadioItemState extends MenuItemState { checked: boolean }
export interface MenuRadioItemProps extends Omit<MenuItemProps, "style" | "onSelect"> { style?: StateStyle<MenuRadioItemState> }
const MenuRadioItem = React.forwardRef<BlendxElement, MenuRadioItemProps>(
  function MenuRadioItem({ style, closeOnSelect = false, ...props }, ref) {
    const radio = React.useContext(MenuRadioContext)
    if (!radio) throw new Error("MenuRadioItem must be used inside MenuRadioGroup")
    const menu = useMenu("MenuRadioItem")
    const state = { highlighted: menu.active === props.value, disabled: props.disabled ?? false, checked: radio.value === props.value }
    return <MenuItem {...props} ref={ref} closeOnSelect={closeOnSelect} style={resolveStyle(style, state)} onSelect={() => radio.setValue(props.value)} />
  },
)
export const DropdownMenuRadioItem = MenuRadioItem
export const ContextMenuRadioItem = MenuRadioItem

const SubmenuParentContext = React.createContext<MenuContextValue | null>(null)

export function DropdownMenuSub({ children, open, defaultOpen = false, onOpenChange, disabled = false }: MenuRootProps) {
  const parent = useMenu("DropdownMenuSub")
  const menu = useMenuState({ open, defaultOpen, onOpenChange, disabled })
  return (
    <SubmenuParentContext.Provider value={parent}>
      <MenuContext.Provider value={menu}>{children}</MenuContext.Provider>
    </SubmenuParentContext.Provider>
  )
}

export interface DropdownMenuSubTriggerProps extends Omit<HostProps, "style"> {
  value: string
  label?: string
  disabled?: boolean
  style?: StateStyle<MenuItemState>
}

export const DropdownMenuSubTrigger = React.forwardRef<BlendxElement, DropdownMenuSubTriggerProps>(
  function DropdownMenuSubTrigger({ value, label = value, disabled = false, style, onMouseEnter, onClick, onKeyDown, ...props }, ref) {
    const menu = useMenu("DropdownMenuSubTrigger")
    const parent = React.useContext(SubmenuParentContext)
    if (!parent) throw new Error("DropdownMenuSubTrigger must be used inside DropdownMenuSub")
    const action = React.useRef(() => {})
    action.current = () => { if (!disabled) menu.setOpen(true) }
    React.useLayoutEffect(() => parent.register({ value, label, disabled, action, submenu: true }), [parent.register, value, label, disabled])
    return (
      <button
        {...props}
        ref={(element) => { menu.setTrigger(element); setRefs(element, ref) }}
        accessibilityRole="menuitem"
        disabled={disabled}
        tabIndex={-1}
        style={resolveStyle(style, { highlighted: parent.active === value, disabled })}
        onMouseEnter={(event) => {
          onMouseEnter?.(event)
          if (!disabled) { parent.setActive(value); menu.setOpen(true) }
        }}
        onClick={(event) => { onClick?.(event); action.current() }}
        onKeyDown={(event) => {
          onKeyDown?.(event)
          const key = normalizeKey(event.key)
          if (key === "right" || key === "enter" || key === "space") menu.setOpen(true)
        }}
      />
    )
  },
)

export const DropdownMenuSubContent = React.forwardRef<BlendxElement, DropdownMenuContentProps>(
  function DropdownMenuSubContent({ onKeyDown, ...props }, ref) {
    const menu = useMenu("DropdownMenuSubContent")
    if (!menu.open || menu.anchorId === undefined) return null
    return (
      <MenuContent
        {...props}
        ref={ref}
        anchorId={menu.anchorId}
        side="right"
        align="start"
        onKeyDown={(event) => {
          onKeyDown?.(event)
          if (normalizeKey(event.key) === "left") menu.setOpen(false)
        }}
      />
    )
  },
)

export const DropdownMenuLabel = React.forwardRef<BlendxElement, HostProps>((props, ref) => <div {...props} ref={ref} />)
export const ContextMenuLabel = DropdownMenuLabel
export const DropdownMenuSeparator = React.forwardRef<BlendxElement, HostProps>((props, ref) => <separator {...props} ref={ref} />)
export const ContextMenuSeparator = DropdownMenuSeparator
