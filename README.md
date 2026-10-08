# MyGUI

A small C++20 GUI library and demo built on GLFW, OpenGL 3.3, and FreeType. It includes owned widget trees, horizontal and vertical layouts, nested clipping, UTF-8 text, labels, and buttons with mouse and keyboard interaction.

Linux is the validated platform. Public headers hide native GLFW, OpenGL, and FreeType types. Windows and macOS are not currently tested.

## Build and run

Install a C++20 compiler, CMake 3.20 or newer, Ninja, GLFW, OpenGL development files, and FreeType.

Ubuntu/Debian:

```sh
sudo apt-get install build-essential cmake ninja-build libglfw3-dev libgl1-mesa-dev libfreetype-dev
```

Fedora:

```sh
sudo dnf install gcc-c++ cmake ninja-build glfw-devel mesa-libGL-devel freetype-devel
```

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/MyGUI
```

Use the `release` preset for a Release build. Font assets are bundled and copied beside the executable, so the demo can be launched from another working directory. `--font PATH` chooses a different scalable Unicode font.

The demo provides an overlay-toggle button, a click counter, a disabled button, and clipped overlapping panels. Hover the overlay, resize the window, or press Tab/Shift+Tab to change focus and Enter/Space to activate a button. V toggles the overlay. `--log-events` enables event logging; `--frames N` exits after N frames for smoke tests. `--width N --height N` selects the starting window size.

## Tests

```sh
ctest --preset debug
```

Renderer and window integration tests require a display. For software rendering on a machine without a display:

```sh
sudo apt-get install xvfb xauth
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --preset debug
```

Input, widget, layout, and font tests run without a display:

```sh
ctest --preset debug -L headless
```

CI builds Debug and Release on Ubuntu and runs every suite under Xvfb/Mesa. The input test executable has no GLFW, OpenGL, or FreeType dependency; the other headless suites link the library but never initialize GLFW or create a context.

To build just the library and demo, configure with `-DBUILD_TESTING=OFF`. To save a renderer-test preview, set `MYGUI_RENDER_SNAPSHOT` to a PPM output path when running `MyGUI_tests`.

## Windows, events, and resource lifetimes

All window operations, input feeding, UI dispatch, and drawing belong on the main thread. The library initializes GLFW for the first `gui::Window` and shuts it down after the last one. Destroying or replacing one window leaves other windows usable. Do not mix this ownership with externally managed native GLFW windows, or destroy windows inside native callbacks.

Call `pollEvents()` on **one** live window once per application frame. GLFW processes events globally, so the call resets pressed/released edges and scroll offsets for every library window, then polls all of them. Held state and cursor positions persist. Callback exceptions are saved and rethrown at the polling boundary, never through GLFW; if several windows have pending exceptions, subsequent polls report the remaining ones. Recursive polling is rejected.

Choose event delivery explicitly when practical:

- `EventDelivery::Queue`: update input and enqueue events; bypass the callback.
- `EventDelivery::Callback`: update input and call the handler; never enqueue.
- `EventDelivery::Both`: enqueue before calling the handler. This is the backward-compatible default.

Changing to callback-only delivery discards unread queued events. Re-enabling a queue records only future events. Queue consumers must drain unread events; callback-only delivery avoids queue accumulation. Using both APIs to dispatch to the same UI would dispatch every event twice, so choose one delivery path for each UI context.

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

Call `window.makeContextCurrent()` before creating, drawing with, or destroying its renderer. `setSwapInterval(1)` makes that window current and enables vsync. Use one renderer per context and declare it after its window so GPU resources are destroyed first. Font objects contain CPU resources and can be shared by renderers; glyph atlases remain owned by each renderer. Window resize callbacks update size/event data without changing any OpenGL viewport; `Renderer::beginFrame()` sets the viewport.

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

This version does not include text editing, scrolling controls, automatic line wrapping, complex-script shaping, bidirectional layout, color emoji, or native accessibility integration. Labels and buttons clip text that exceeds their bounds. The bundled Noto Sans font's pinned source and SIL Open Font License are in `assets/fonts/SOURCE.md` and `assets/fonts/OFL.txt`.
