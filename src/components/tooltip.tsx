import React from "react"
import type { BlendxElement, HostProps } from "../types.js"
import { FloatingLayer, setRefs, useControllableState } from "./floating.js"
import type { FloatingContentProps } from "./floating.js"

type TooltipContextValue = {
  open: boolean
  anchorId?: number
  setAnchor: (element: BlendxElement | null) => void
  scheduleOpen: () => void
  scheduleClose: () => void
  cancelClose: () => void
  close: () => void
}

const TooltipContext = React.createContext<TooltipContextValue | null>(null)
const TooltipProviderContext = React.createContext({ delayDuration: 250 })

function useTooltip(name: string) {
  const context = React.useContext(TooltipContext)
  if (!context) throw new Error(`${name} must be used inside Tooltip`)
  return context
}

export function TooltipProvider({ children, delayDuration = 250 }: { children: React.ReactNode; delayDuration?: number }) {
  return <TooltipProviderContext.Provider value={{ delayDuration }}>{children}</TooltipProviderContext.Provider>
}

export interface TooltipProps {
  children?: React.ReactNode
  open?: boolean
  defaultOpen?: boolean
  onOpenChange?: (open: boolean) => void
  delayDuration?: number
}

export function Tooltip({ children, open: openProp, defaultOpen = false, onOpenChange, delayDuration }: TooltipProps) {
  const provider = React.useContext(TooltipProviderContext)
  const [open, setOpen] = useControllableState({ value: openProp, defaultValue: defaultOpen, onChange: onOpenChange })
  const [anchorId, setAnchorId] = React.useState<number>()
  const openTimer = React.useRef<ReturnType<typeof setTimeout> | null>(null)
  const closeTimer = React.useRef<ReturnType<typeof setTimeout> | null>(null)
  const clear = (timer: React.MutableRefObject<ReturnType<typeof setTimeout> | null>) => {
    if (timer.current !== null) clearTimeout(timer.current)
    timer.current = null
  }
  const cancelClose = () => clear(closeTimer)
  const close = () => { clear(openTimer); clear(closeTimer); setOpen(false) }
  const scheduleOpen = () => {
    clear(closeTimer)
    clear(openTimer)
    const wait = delayDuration ?? provider.delayDuration
    if (wait <= 0) setOpen(true)
    else openTimer.current = setTimeout(() => setOpen(true), wait)
  }
  const scheduleClose = () => {
    clear(openTimer)
    clear(closeTimer)
    closeTimer.current = setTimeout(() => setOpen(false), 90)
  }
  React.useEffect(() => () => { clear(openTimer); clear(closeTimer) }, [])
  const value = React.useMemo(() => ({
    open,
    anchorId,
    setAnchor: (element: BlendxElement | null) => setAnchorId(element?.id),
    scheduleOpen,
    scheduleClose,
    cancelClose,
    close,
  }), [open, anchorId, delayDuration, provider.delayDuration])
  return <TooltipContext.Provider value={value}>{children}</TooltipContext.Provider>
}

export interface TooltipTriggerProps extends HostProps {}

export const TooltipTrigger = React.forwardRef<BlendxElement, TooltipTriggerProps>(
  function TooltipTrigger({ onMouseEnter, onMouseLeave, onFocus, onBlur, onMouseDown, ...props }, forwardedRef) {
    const tooltip = useTooltip("TooltipTrigger")
    return (
      <div
        {...props}
        ref={(element) => { tooltip.setAnchor(element); setRefs(element, forwardedRef) }}
        tabIndex={props.tabIndex ?? 0}
        onMouseEnter={(event) => { onMouseEnter?.(event); tooltip.scheduleOpen() }}
        onMouseLeave={(event) => { onMouseLeave?.(event); tooltip.scheduleClose() }}
        onFocus={(event) => { onFocus?.(event); tooltip.scheduleOpen() }}
        onBlur={(event) => { onBlur?.(event); tooltip.close() }}
        onMouseDown={(event) => { onMouseDown?.(event); tooltip.close() }}
      />
    )
  },
)

export interface TooltipContentProps extends FloatingContentProps {}

export const TooltipContent = React.forwardRef<BlendxElement, TooltipContentProps>(
  function TooltipContent({ onMouseEnter, onMouseLeave, ...props }, ref) {
    const tooltip = useTooltip("TooltipContent")
    if (!tooltip.open || tooltip.anchorId === undefined) return null
    return (
      <FloatingLayer
        {...props}
        ref={ref}
        anchorId={tooltip.anchorId}
        onMouseEnter={(event) => { onMouseEnter?.(event); tooltip.cancelClose() }}
        onMouseLeave={(event) => { onMouseLeave?.(event); tooltip.scheduleClose() }}
      />
    )
  },
)
