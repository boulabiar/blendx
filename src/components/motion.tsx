import React from "react"
import type { BlendxElement, HostProps, Style } from "../types.js"

export interface MotionTransition {
  duration?: number
  delay?: number
  easing?: "linear" | "easeIn" | "easeOut" | "easeInOut"
}

export interface MotionProps extends HostProps {
  initial?: Partial<Style>
  animate?: Partial<Style>
  transition?: MotionTransition
  onAnimationComplete?: () => void
}

const animatedKeys = ["width", "height", "left", "right", "top", "bottom", "opacity", "borderRadius"] as const

function easing(name: MotionTransition["easing"], value: number): number {
  if (name === "linear") return value
  if (name === "easeIn") return value * value
  if (name === "easeOut") return 1 - (1 - value) * (1 - value)
  return value < 0.5 ? 2 * value * value : 1 - Math.pow(-2 * value + 2, 2) / 2
}

function createMotionElement(type: "div" | "text" | "button") {
  return React.forwardRef<BlendxElement, MotionProps>(function MotionElement(
    { initial, animate, transition, style, onAnimationComplete, ...props },
    ref,
  ) {
    const target = React.useMemo(() => ({ ...style, ...animate }), [animate, style])
    const current = React.useRef<Style>({ ...style, ...initial })
    const [rendered, setRendered] = React.useState<Style>(current.current)

    React.useEffect(() => {
      const from = current.current
      const duration = Math.max(0, transition?.duration ?? 220)
      const delay = Math.max(0, transition?.delay ?? 0)
      const started = performance.now() + delay
      let timer: ReturnType<typeof setTimeout> | null = null
      const frame = () => {
        const now = performance.now()
        const progress = duration === 0 ? 1 : Math.max(0, Math.min(1, (now - started) / duration))
        const amount = easing(transition?.easing, progress)
        const next: Style = { ...target }
        for (const key of animatedKeys) {
          const startValue = from[key]
          const endValue = target[key]
          if (typeof startValue === "number" && typeof endValue === "number") {
            ;(next as Record<string, unknown>)[key] = startValue + (endValue - startValue) * amount
          }
        }
        current.current = next
        setRendered(next)
        if (progress < 1) timer = setTimeout(frame, 16)
        else onAnimationComplete?.()
      }
      timer = setTimeout(frame, delay > 0 ? Math.min(delay, 16) : 0)
      return () => { if (timer) clearTimeout(timer) }
    }, [onAnimationComplete, target, transition?.delay, transition?.duration, transition?.easing])

    return React.createElement(type, { ...props, ref, style: rendered })
  })
}

export const motion = {
  div: createMotionElement("div"),
  text: createMotionElement("text"),
  button: createMotionElement("button"),
}
