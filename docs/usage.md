# MyGUI usage reference

See the [README](../README.md) for installation, build commands, and demo controls.
For a complete application, see [the demo source](../src/main.cpp).

- [Windows and resource lifetimes](#windows-and-resource-lifetimes)
- [Widget ownership and layout](#widget-ownership-and-layout)
- [Interaction and extending widgets](#interaction-and-extending-widgets)
- [Text support and current limits](#text-support-and-current-limits)
- [Test details](#test-details)

## Windows and resource lifetimes

All window operations, input feeding, UI dispatch, and drawing belong on the main thread. The library initializes GLFW for the first `gui::Window` and shuts it down after the last one. Destroying or replacing one window leaves other windows usable. Do not mix this ownership with externally managed native GLFW windows, or destroy windows inside native callbacks.

### Polling and input state

Call `pollEvents()` on **one** live window once per application frame. GLFW processes events globally, so the call resets pressed/released edges and scroll offsets for every library window, then polls all of them. Held state and cursor positions persist. Movement deltas reset at the same boundary
and accumulate signed logical-coordinate differences across mouse movement, button,
and scroll position samples. The first sample establishes a baseline without movement.
Focus changes and cursor entry/leave invalidate that baseline; the next sample avoids a re-entry
jump while retaining any movement already recorded in the frame. Focus loss clears
held keys/buttons and marks release edges, without manufacturing release events. Callback exceptions are saved and rethrown at the polling boundary, never through GLFW; if several windows have pending exceptions, subsequent polls report the remaining ones. Recursive polling is rejected.

### Event delivery

Choose event delivery explicitly when practical:

- `EventDelivery::Queue`: update input and enqueue events; bypass the callback.
- `EventDelivery::Callback`: update input and call the handler; never enqueue.
- `EventDelivery::Both`: enqueue before calling the handler. This is the backward-compatible default.

Changing to callback-only delivery discards unread queued events. Re-enabling a queue records only future events.
Callbacks may clear or replace themselves; their callable state persists across events
without copying the callable for every delivery.

The queue owns copies of events and returns values from `nextEvent()`; returned events
remain valid after window destruction. Callback event references are valid only during
the call, so copy an event to retain it. `gui::EventQueue` is also usable independently
of a window: it starts with 64 circular slots, doubles when full, and reuses capacity
when drained or cleared. It never coalesces or silently drops events. Memory allocation
failure is reported through the existing deferred-exception boundary for window delivery.

Queue consumers must drain unread events; callback-only delivery avoids queue accumulation. Using both APIs to dispatch to the same UI would dispatch every event twice, so choose one delivery path for each UI context.

```cpp
window.setEventDelivery(gui::EventDelivery::Queue);
window.pollEvents();
ui.layout(window.viewport());
while (auto event = window.nextEvent()) {
    if (!ui.handleEvent(*event)) {
        // Handle application shortcuts here.
    }
}
```

### Rendering and window size

Call `window.makeContextCurrent()` before creating, drawing with, or destroying its renderer. `setSwapInterval(1)` makes that window current and enables vsync. Use one renderer per context and declare it after its window so GPU resources are destroyed first. Font objects contain CPU resources and can be shared by renderers; glyph atlases remain owned by each renderer. Window resize callbacks update cached size/event data without changing any OpenGL viewport.
`WindowResizeEvent::source` distinguishes `ResizeSource::Window` from
`ResizeSource::Framebuffer`; both retain `EventType::WindowResized` and contain the
latest logical and framebuffer dimensions. Framebuffer callbacks also query the native
logical dimensions because some backends can omit a logical-size callback when a
hidden window is resized. `viewport()` returns the same cached snapshot; `Renderer::beginFrame()` sets the viewport.

The renderer manages its drawing state rather than restoring another graphics engine's bindings. Frames use `beginFrame`, `clear`, draw calls, and `endFrame`. Clearing discards earlier queued drawing and clears the whole framebuffer, regardless of the active clip. Clip pushes intersect their parents; pops must balance before `endFrame`. A new frame resets the clip stack. Drawing uses logical coordinates, while scissor rectangles and glyph rasterization account for independent framebuffer X/Y scales.

## Widget ownership and layout

`UIContext` owns its root `Container`. A container owns children with `std::unique_ptr`; `emplace<T>()` returns a reference valid until the child is removed. Children draw in insertion order, so later children appear on top. Widget copies retain visual properties but not their parent, UI attachment, focus, hover, capture, or pressed state. Containers cannot be copied or moved.

```cpp
const auto font = gui::Font::load("assets/fonts/NotoSans.ttf");
gui::UIContext ui;
ui.root().setLayout(gui::Layout::Vertical);
ui.root().setPadding({16, 16, 16, 16});
ui.root().setSpacing(8);
auto& label = ui.root().emplace<gui::Label>("Hello", font);
auto& button = ui.root().emplace<gui::Button>("Click", font);
button.setOnClick([&] { label.setText("Clicked!"); });
```

Bounds are always in logical **window** coordinates, including nested children. Horizontal/vertical layouts calculate them from the container's content area. Preferred sizes shrink toward minimum sizes when space is limited; flex weights distribute surplus main-axis space. Children fill the cross axis while retaining their minimum dimensions. If minimum sizes exceed available space, children overflow and the container clips them. Hidden widgets reserve no space; disabled widgets still do. Absolute containers preserve explicitly supplied child bounds. Override `Container::arrange()` for custom relative artwork, as the demo gallery does.

Containers clip children to their padded content area by default. `setClipChildren(false)` allows overflow drawing and hit testing, while ancestor clips still apply. Text and size changes invalidate layout; call `ui.layout(viewport)` to provide the current window dimensions. Layout is refreshed before dispatch and drawing.

## Interaction and extending widgets

The UI routes pointer events to the topmost visible target within its ancestor clips, then bubbles unconsumed events to its parents. Disabled targets block underlying widgets. Keyboard events go to the focused widget. Tab traversal uses tree order and skips hidden/disabled widgets. Captured pointer motion/releases reach the capturing widget outside its bounds; cursor leave clears hover, while window focus loss cancels focus, capture, and pressed state.

Derive from `Widget` to implement `draw(Renderer&)` and optionally `onEvent(const Event&, UIContext&)`. Return `true` when an event is consumed. Use `requestFocus`, `capturePointer`, and `releasePointer` for interactions; implement the nonthrowing `cancelInteraction()` hook to reset custom pressed or drag state. Hiding, disabling, or removing an ancestor cancels interactions in its subtree.

Container additions/removals during dispatch are committed after the handler returns. Removal cancels interaction immediately, but leaves the object alive until dispatch finishes, making self-removing callbacks safe. Do not retain references after removal completes. Button callbacks may replace themselves; activation copies the callback before invoking it.

Buttons activate on a matching left-button release inside the visible hit area, or on a matching Enter/Space release while focused. Dragging outside, focus loss, or disabling cancels activation. Keyboard repeats do not activate. Styles include normal, hovered, pressed, disabled, and focus colors.

## Text support and current limits

Fonts are loaded through FreeType. Sizes are logical pixels in `(0, 4096]`, with scaled raster sizes capped at 4096 pixels. `measureText()` returns advance width, line height, ascent, and descent; measurement is independent of framebuffer scale. UTF-8 text supports explicit newlines and four-space tabs. Invalid UTF-8 bytes become U+FFFD; unsupported characters use U+FFFD, `?`, or the font's missing-glyph entry. Font loading and invalid sizes report exceptions.

Glyph coverage textures are cached per renderer, font, and raster size. Atlas pages remain fixed while queued commands reference them. The cache lasts for the renderer's lifetime; applications displaying many different font sizes should bound their font-size choices.

This version does not include text editing, scrolling controls, automatic line wrapping, complex-script shaping, bidirectional layout, color emoji, or native accessibility integration. Labels and buttons clip text that exceeds their bounds. The bundled Noto Sans font's pinned source and SIL Open Font License are in [font source](../assets/fonts/SOURCE.md) and [SIL Open Font License](../assets/fonts/OFL.txt).

## Test details

CI builds Debug and Release on Ubuntu and runs every suite under Xvfb/Mesa.
The input test executable has no GLFW, OpenGL, or FreeType dependency. The other
headless suites link the library but never initialize GLFW or create a context.

To build just the library and demo, disable tests when configuring:

```sh
cmake --preset debug -DBUILD_TESTING=OFF
cmake --build --preset debug
```

To enable tests again, configure with `cmake --preset debug` and rebuild.
To save a renderer-test preview, set `MYGUI_RENDER_SNAPSHOT` to a PPM output path
when running `MyGUI_tests`.

The demo's input diagnostics show logical mouse coordinates, accumulated movement
and scroll, held/pressed/released states for the left mouse button and V key, and
the last key, button, or text event. Single-frame edges can be brief; use
`--log-events` to record every transition, including presses and releases within
the same frame. Logs include event names, coordinates, actions, modifiers, Unicode
code points, and resize sources.
