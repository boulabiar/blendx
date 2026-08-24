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
- `virtual-list` supports fixed-height `followTail`, top/bottom alignment, and
  native `scrollToItem()` in addition to wheel scrolling.

Variable-height memory-windowed lists, full text selection/editing, modal focus
trapping, native motion, accessibility nodes, and IME composition remain
separate deeper renderer projects.

## Selection and value controls

`Checkbox`, `Switch`, `RadioGroup`, and `Slider` support controlled and
uncontrolled state, disabled states, focus styling, pointer interaction, and
keyboard activation. Radio groups use arrow-key selection. Sliders support
single values or two-thumb ranges, step snapping, pointer dragging, arrows,
Home, and End.

Run the visual control surface with `npm run controls`.
