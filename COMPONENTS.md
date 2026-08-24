# BlendX components

BlendX includes compound floating controls built over native element refs,
pointer events, focus, and anchored layers. They are exported from the main
`blendx` entry.

## Tooltip

```tsx
<TooltipProvider delayDuration={200}>
  <Tooltip>
    <TooltipTrigger style={{ width: 120, height: 36 }}>
      <text>Hover me</text>
    </TooltipTrigger>
    <TooltipContent side="top" align="center" sideOffset={8}
      style={{ width: 160, height: 40, position: "absolute" }}>
      <text>Native tooltip</text>
    </TooltipContent>
  </Tooltip>
</TooltipProvider>
```

Hover and focus open the tooltip. Moving away, blur, or pointer-down closes it.
The content remains hoverable during the short close grace period.

## Select

```tsx
<Select defaultValue="hermes" onValueChange={setRuntime}>
  <SelectTrigger style={{ width: 220, height: 38 }}>
    <SelectValue placeholder="Choose a runtime" />
  </SelectTrigger>
  <SelectContent side="bottom" sideOffset={6}
    style={{ width: 220, padding: 4, position: "absolute" }}>
    <SelectItem value="hermes">Hermes</SelectItem>
    <SelectItem value="v8">V8</SelectItem>
  </SelectContent>
</Select>
```

Select supports controlled or uncontrolled value/open state, disabled items,
pointer highlighting, arrow navigation, Enter/Space selection, Escape, outside
dismissal, and focus return to the trigger.

## Combobox

```tsx
<Combobox items={["React", "Vue", "Svelte"]} onValueChange={setFramework}>
  <ComboboxInput placeholder="Search frameworks" />
  <ComboboxContent side="bottom" style={{ width: 220, position: "absolute" }}>
    <ComboboxEmpty>No matches</ComboboxEmpty>
    <ComboboxList>
      {(item) => <ComboboxItem key={item} value={item}>{item}</ComboboxItem>}
    </ComboboxList>
  </ComboboxContent>
</Combobox>
```

Combobox supports controlled input/value/open state, custom filtering,
single or multiple selection, arrow navigation, Enter selection, and outside
dismissal.

## Interaction foundation

- `ref` exposes a stable `{ id, type }` native element handle.
- `anchored.anchorId` positions a floating layer from an element rather than a
  hard-coded point.
- `style.hover` and `style.active` apply pointer-state overrides.
- `onMouseEnter`, `onMouseLeave`, `onMouseDownOutside`, and `onKeyUp` are
  supported native events.
- `tabIndex`, Tab/Shift+Tab traversal, programmatic `focusElement()`, and
  Enter/Space button activation are supported.
- `VirtualList` memory-windows React and native rows, supports known variable
  heights, follow-tail, visible-range reporting, scroll anchoring, and
  imperative index/offset scrolling.
- Input and textarea elements provide selection, clipboard, navigation,
  undo/redo, password masking, and IME pre-edit display.
- Accessibility roles, labels, disabled state, selection, checked state, and
  control values are inspectable through `renderer.getAccessibilityTree()`;
  OS accessibility adapters remain future platform work.

## Selection and value controls

`Checkbox`, `Switch`, `RadioGroup`, and `Slider` support controlled and
uncontrolled state, disabled states, focus styling, pointer interaction, and
keyboard activation. Radio groups use arrow-key selection. Sliders support
single values or two-thumb ranges, step snapping, pointer dragging, arrows,
Home, and End.

Run the visual control surface with `npm run controls`.

## Navigation and disclosure

`Tabs` supports horizontal or vertical orientation, automatic or manual
activation, disabled triggers, roving focus, arrows, Home, and End.

`Accordion` supports single or multiple expanded items, controlled and
uncontrolled state, collapsible single items, disabled items, and keyboard
focus navigation. `Collapsible` provides the smaller standalone disclosure
primitive.

Run the combined workspace-settings example with `npm run disclosure`.

## Menus

`DropdownMenu` anchors to its trigger. `ContextMenu` opens at the native
right-click position. Both support controlled or uncontrolled open state,
disabled items, hover highlighting, arrows, Home, End, Enter/Space, Escape,
single-character typeahead, outside dismissal, and focus restoration.

Checkbox and radio menu items retain selection without closing by default.
Nested submenus support pointer opening and Right/Left keyboard traversal.
Labels and separators are shared structural primitives. Run the file-workspace
example with `npm run menus`.

## Dialog and toast

`Dialog` provides controlled or uncontrolled state, trigger/close primitives,
an outside-dismissable backdrop, Escape dismissal, modal Tab trapping, and
focus restoration. `ToastProvider`, `useToast()`, and `ToastViewport` manage a
timed notification queue with programmatic dismissal.

## Virtual lists and motion

`VirtualList<T>` accepts `items`, `renderItem`, `estimatedItemHeight`, and an
optional `getItemHeight`. Its ref exposes `scrollToIndex()` and
`scrollToOffset()`. Only visible rows plus overdraw exist in the React/native
tree, so large data sets do not inflate renderer memory.

`motion.div`, `motion.text`, and `motion.button` interpolate numeric style
properties using declarative `initial`, `animate`, and `transition` props. The
timeline is currently driven by React frames rather than a native compositor.

Run `npm run foundation` for a combined visual gallery of `Dialog`, toast,
motion, advanced editor input, selectable wrapped text, accessibility-tree
inspection, and a variable-height `VirtualList`.
