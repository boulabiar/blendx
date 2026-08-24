import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { BlendxElement, HostProps, Style } from "../types.js"
import { normalizeKey, setRefs, useControllableState } from "./floating.js"

type DialogContextValue = {
  open: boolean
  setOpen: (open: boolean) => void
  trigger: React.MutableRefObject<BlendxElement | null>
}

const DialogContext = React.createContext<DialogContextValue | null>(null)

function useDialog(name: string): DialogContextValue {
  const dialog = React.useContext(DialogContext)
  if (!dialog) throw new Error(`${name} must be used inside Dialog`)
  return dialog
}

export interface DialogProps {
  children?: React.ReactNode
  open?: boolean
  defaultOpen?: boolean
  onOpenChange?: (open: boolean) => void
}

export function Dialog({ children, open, defaultOpen = false, onOpenChange }: DialogProps) {
  const [current, setCurrent] = useControllableState({ value: open, defaultValue: defaultOpen, onChange: onOpenChange })
  const trigger = React.useRef<BlendxElement | null>(null)
  const setOpen = React.useCallback((next: boolean) => {
    setCurrent(next)
    if (!next && trigger.current) loadNativeRenderer().focusElement(trigger.current.id)
  }, [setCurrent])
  return <DialogContext.Provider value={{ open: current, setOpen, trigger }}>{children}</DialogContext.Provider>
}

export const DialogTrigger = React.forwardRef<BlendxElement, HostProps>(
  function DialogTrigger({ onClick, ...props }, ref) {
    const dialog = useDialog("DialogTrigger")
    return (
      <button
        {...props}
        ref={(element) => { dialog.trigger.current = element; setRefs(element, ref) }}
        onClick={(event) => { onClick?.(event); dialog.setOpen(true) }}
      />
    )
  },
)

export interface DialogContentProps extends HostProps {
  overlayStyle?: Style
  closeOnOutside?: boolean
}

export const DialogContent = React.forwardRef<BlendxElement, DialogContentProps>(
  function DialogContent({ overlayStyle, closeOnOutside = true, onKeyDown, onMouseDown, style, ...props }, ref) {
    const dialog = useDialog("DialogContent")
    if (!dialog.open) return null
    return (
      <div
        modal
        accessibilityRole="dialog"
        style={{
          width: "100%",
          height: "100%",
          position: "fixed",
          left: 0,
          top: 0,
          zIndex: 1000,
          alignItems: "center",
          justifyContent: "center",
          backgroundColor: "#00000080",
          ...overlayStyle,
        }}
        onMouseDown={() => { if (closeOnOutside) dialog.setOpen(false) }}
      >
        <div
          {...props}
          ref={ref}
          autoFocus
          tabIndex={props.tabIndex ?? 0}
          style={style}
          onMouseDown={(event) => onMouseDown?.(event)}
          onKeyDown={(event) => {
            onKeyDown?.(event)
            if (normalizeKey(event.key) === "escape") dialog.setOpen(false)
          }}
        />
      </div>
    )
  },
)

export function DialogClose({ onClick, ...props }: HostProps) {
  const dialog = useDialog("DialogClose")
  return <button {...props} onClick={(event) => { onClick?.(event); dialog.setOpen(false) }} />
}
