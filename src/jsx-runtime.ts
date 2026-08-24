import type React from "react"
import type { HostProps } from "./types.js"

export { Fragment, jsx, jsxs } from "react/jsx-runtime"

export namespace JSX {
  export type ElementType = React.JSX.ElementType
  export type Element = React.JSX.Element
  export type ElementClass = React.JSX.ElementClass
  export type ElementAttributesProperty = React.JSX.ElementAttributesProperty
  export type ElementChildrenAttribute = React.JSX.ElementChildrenAttribute
  export type IntrinsicAttributes = React.JSX.IntrinsicAttributes
  export type IntrinsicClassAttributes<T> = React.JSX.IntrinsicClassAttributes<T>

  export interface IntrinsicElements {
    div: HostProps
    text: HostProps
    "virtual-list": HostProps
  }
}
