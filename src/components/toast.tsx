import React from "react"
import type { HostProps } from "../types.js"

export interface ToastOptions {
  title: string
  description?: string
  duration?: number
}

type ToastRecord = ToastOptions & { id: number }
type ToastContextValue = {
  items: ToastRecord[]
  toast: (options: ToastOptions) => number
  dismiss: (id: number) => void
}

const ToastContext = React.createContext<ToastContextValue | null>(null)

export function ToastProvider({ children }: { children?: React.ReactNode }) {
  const [items, setItems] = React.useState<ToastRecord[]>([])
  const nextId = React.useRef(1)
  const timers = React.useRef(new Map<number, ReturnType<typeof setTimeout>>())
  const dismiss = React.useCallback((id: number) => {
    const timer = timers.current.get(id)
    if (timer) clearTimeout(timer)
    timers.current.delete(id)
    setItems((current) => current.filter((item) => item.id !== id))
  }, [])
  const toast = React.useCallback((options: ToastOptions) => {
    const id = nextId.current++
    setItems((current) => [...current, { ...options, id }])
    if (options.duration !== 0) {
      timers.current.set(id, setTimeout(() => dismiss(id), options.duration ?? 3500))
    }
    return id
  }, [dismiss])
  React.useEffect(() => () => { for (const timer of timers.current.values()) clearTimeout(timer) }, [])
  return <ToastContext.Provider value={{ items, toast, dismiss }}>{children}</ToastContext.Provider>
}

export function useToast(): Pick<ToastContextValue, "toast" | "dismiss"> {
  const context = React.useContext(ToastContext)
  if (!context) throw new Error("useToast must be used inside ToastProvider")
  return context
}

export interface ToastViewportProps extends HostProps {
  renderToast?: (toast: ToastRecord, dismiss: () => void) => React.ReactNode
}

export function ToastViewport({ renderToast, style, ...props }: ToastViewportProps) {
  const context = React.useContext(ToastContext)
  if (!context) throw new Error("ToastViewport must be used inside ToastProvider")
  return (
    <div
      {...props}
      accessibilityRole="status"
      style={{ width: 320, position: "fixed", right: 18, top: 18, zIndex: 1100, gap: 8, ...style }}
    >
      {context.items.map((item) => renderToast
        ? <React.Fragment key={item.id}>{renderToast(item, () => context.dismiss(item.id))}</React.Fragment>
        : (
          <button
            key={item.id}
            onClick={() => context.dismiss(item.id)}
            style={{ width: "100%", padding: 12, gap: 4, backgroundColor: "#202838", borderColor: "#39465a", borderWidth: 1, borderRadius: 9 }}
          >
            <text style={{ color: "#f4f7fb", fontSize: 13 }}>{item.title}</text>
            {item.description ? <text style={{ color: "#95a1b3", fontSize: 11, whiteSpace: "normal" }}>{item.description}</text> : null}
          </button>
        ))}
    </div>
  )
}
