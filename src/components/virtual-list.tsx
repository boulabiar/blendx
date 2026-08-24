import React from "react"
import { loadNativeRenderer } from "../native.js"
import type { BlendxElement, BlendxEvent, HostProps } from "../types.js"

export interface VirtualListHandle {
  scrollToIndex(index: number, align?: "start" | "center" | "end" | "nearest"): void
  scrollToOffset(offset: number): void
}

export interface VirtualListProps<T> extends Omit<HostProps, "children" | "onScroll" | "ref"> {
  items: readonly T[]
  renderItem: (item: T, index: number) => React.ReactNode
  estimatedItemHeight: number
  getItemHeight?: (item: T, index: number) => number
  getItemKey?: (item: T, index: number) => React.Key
  overdraw?: number
  followTail?: boolean
  onScroll?: (event: BlendxEvent) => void
  onVisibleRangeChange?: (start: number, end: number) => void
}

function firstItemAfter(offsets: readonly number[], value: number): number {
  let low = 0
  let high = offsets.length - 1
  while (low < high) {
    const middle = Math.floor((low + high) / 2)
    if ((offsets[middle + 1] ?? Infinity) <= value) low = middle + 1
    else high = middle
  }
  return low
}

function VirtualListInner<T>(
  {
    items,
    renderItem,
    estimatedItemHeight,
    getItemHeight,
    getItemKey,
    overdraw = 3,
    followTail = false,
    onScroll,
    onVisibleRangeChange,
    style,
    ...props
  }: VirtualListProps<T>,
  forwardedRef: React.ForwardedRef<VirtualListHandle>,
) {
  const element = React.useRef<BlendxElement | null>(null)
  const initialViewport = typeof style?.height === "number" ? style.height : estimatedItemHeight * 10
  const [viewport, setViewport] = React.useState(Math.max(1, initialViewport))
  const [scrollOffset, setScrollOffset] = React.useState(0)
  const previousLength = React.useRef(items.length)
  const previousGeometry = React.useRef<{ offsets: readonly number[]; first: number; scrollOffset: number } | undefined>(undefined)

  const { heights, offsets, totalHeight } = React.useMemo(() => {
    const nextHeights = items.map((item, index) => Math.max(1, getItemHeight?.(item, index) ?? estimatedItemHeight))
    const nextOffsets = new Array<number>(nextHeights.length + 1)
    nextOffsets[0] = 0
    for (let index = 0; index < nextHeights.length; index += 1) {
      nextOffsets[index + 1] = nextOffsets[index]! + nextHeights[index]!
    }
    return { heights: nextHeights, offsets: nextOffsets, totalHeight: nextOffsets[nextHeights.length] ?? 0 }
  }, [items, getItemHeight, estimatedItemHeight])

  const maximumOffset = Math.max(0, totalHeight - viewport)
  const clampedOffset = Math.min(scrollOffset, maximumOffset)
  const first = items.length ? firstItemAfter(offsets, clampedOffset) : 0
  const last = items.length ? Math.min(items.length, firstItemAfter(offsets, clampedOffset + viewport) + 1) : 0
  const start = Math.max(0, first - overdraw)
  const end = Math.min(items.length, last + overdraw)

  const setNativeOffset = React.useCallback((offset: number) => {
    const next = Math.max(0, Math.min(offset, Math.max(0, totalHeight - viewport)))
    setScrollOffset(next)
    if (element.current) loadNativeRenderer().scrollToOffset(element.current.id, next)
  }, [totalHeight, viewport])

  React.useLayoutEffect(() => {
    const previous = previousGeometry.current
    let anchoredOffset = scrollOffset
    if (previous && previous.offsets !== offsets && previous.first < items.length) {
      const oldTop = previous.offsets[previous.first] ?? 0
      const newTop = offsets[previous.first] ?? 0
      const delta = newTop - oldTop
      if (delta !== 0) {
        anchoredOffset = previous.scrollOffset + delta
        setNativeOffset(anchoredOffset)
      }
    }
    previousGeometry.current = { offsets, first, scrollOffset: anchoredOffset }
  }, [first, items.length, offsets, scrollOffset, setNativeOffset])

  React.useImperativeHandle(forwardedRef, () => ({
    scrollToOffset: setNativeOffset,
    scrollToIndex(index, align = "nearest") {
      if (!items.length) return
      const target = Math.max(0, Math.min(Math.trunc(index), items.length - 1))
      const top = offsets[target] ?? 0
      const bottom = offsets[target + 1] ?? top
      let next = top
      if (align === "center") next = top - (viewport - (bottom - top)) * 0.5
      else if (align === "end") next = bottom - viewport
      else if (align === "nearest") {
        if (top >= scrollOffset && bottom <= scrollOffset + viewport) return
        next = top < scrollOffset ? top : bottom - viewport
      }
      setNativeOffset(next)
    },
  }), [items.length, offsets, scrollOffset, setNativeOffset, viewport])

  React.useLayoutEffect(() => {
    const timer = setTimeout(() => {
      if (!element.current) return
      const renderer = loadNativeRenderer()
      renderer.renderFrame()
      const box = renderer.getElementBox(element.current.id)
      if (box.height > 0) setViewport(box.height)
    }, 0)
    return () => clearTimeout(timer)
  }, [style?.height])

  React.useLayoutEffect(() => {
    const grew = items.length > previousLength.current
    previousLength.current = items.length
    if (!followTail || !grew) return
    const timer = setTimeout(() => {
      loadNativeRenderer().renderFrame()
      setNativeOffset(totalHeight)
    }, 0)
    return () => clearTimeout(timer)
  }, [followTail, items.length, setNativeOffset, totalHeight])

  React.useEffect(() => onVisibleRangeChange?.(start, end), [end, onVisibleRangeChange, start])

  const rows: React.ReactNode[] = []
  for (let index = start; index < end; index += 1) {
    const item = items[index]!
    rows.push(
      <div
        key={getItemKey?.(item, index) ?? index}
        style={{ width: "100%", height: heights[index], flexShrink: 0 }}
      >
        {renderItem(item, index)}
      </div>,
    )
  }

  return (
    <div
      {...props}
      ref={element}
      style={{ ...style, overflow: "scroll" }}
      onScroll={(event) => {
        setScrollOffset(event.scrollOffset ?? event.scrollTarget ?? clampedOffset)
        if (event.viewportSize && event.viewportSize !== viewport) setViewport(event.viewportSize)
        onScroll?.(event)
      }}
    >
      <div style={{ width: "100%", height: offsets[start] ?? 0, flexShrink: 0 }} />
      {rows}
      <div style={{ width: "100%", height: Math.max(0, totalHeight - (offsets[end] ?? 0)), flexShrink: 0 }} />
    </div>
  )
}

export const VirtualList = React.forwardRef(VirtualListInner) as <T>(
  props: VirtualListProps<T> & { ref?: React.Ref<VirtualListHandle> },
) => React.ReactElement
