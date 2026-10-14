# MyGUI

A small C++20 GUI library and demo built on GLFW, OpenGL 3.3, and FreeType. It includes owned widget trees, horizontal and vertical layouts with height-for-width text wrapping, nested clipping, scrolling, UTF-8 text entry with selection and clipboard, themes, images, and labels, buttons, checkboxes, switches, sliders and progress bars with mouse and keyboard interaction.

Linux is the validated platform: CI builds Debug and Release and runs every test, including OpenGL rendering under Xvfb/Mesa. CI also builds everything on Windows (MSVC with vcpkg) and macOS (Homebrew) and runs the tests that need no display there; rendering and window behavior on those platforms are not yet tested. Public headers hide native GLFW, OpenGL, and FreeType types.

## Build and run

Install a C++20 compiler, CMake 3.20 or newer, Ninja, GLFW, and FreeType. OpenGL functions are loaded at runtime through GLFW, so the library has no OpenGL link dependency; the vendored [Khronos headers](third_party/khronos/SOURCE.md) supply the declarations.

Ubuntu/Debian:

```sh
sudo apt-get install build-essential cmake ninja-build libglfw3-dev libgl1-mesa-dev libfreetype-dev
```

Fedora:

```sh
sudo dnf install gcc-c++ cmake ninja-build glfw-devel mesa-libGL-devel freetype-devel
```

macOS (Homebrew):

```sh
brew install cmake ninja glfw freetype
```

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/MyGUI
```

Windows (Visual Studio with [vcpkg](https://vcpkg.io)):

```powershell
vcpkg install glfw3 freetype --triplet x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
.\build\Debug\MyGUI.exe
```

Use the `release` preset for a Release build. Font assets are bundled and copied beside the executables, so the demo and examples can be launched from another working directory. `--font PATH` chooses a different scalable Unicode font for the demo.

## Using MyGUI from another project

Install the library and use the exported CMake package:

```sh
cmake --install build/release --prefix /path/to/prefix
```

```cmake
find_package(MyGUI 0.1 REQUIRED)
target_link_libraries(app PRIVATE MyGUI::MyGUI)
```

The package installs the public headers under `include/gui`, the static library, and the bundled Noto Sans font under `share/MyGUI/fonts`. Because MyGUI is static, its package finds GLFW and FreeType for you. `tests/package` is a minimal consumer that CI builds against a fresh install. When MyGUI is added with `add_subdirectory`, link `MyGUI::MyGUI`; its tests are not built, and examples and install rules default to off (`MYGUI_BUILD_EXAMPLES`, `MYGUI_INSTALL`).

## Examples

Small programs in `examples/` show one topic each and build next to the demo (`-DMYGUI_BUILD_EXAMPLES=OFF` skips them):

- `MyGUI_hello` (`examples/hello.cpp`): a label and a button, the minimum setup.
- `MyGUI_layout` (`examples/layout.cpp`): nested layouts, flex weights and wrapped text that reflows on resize.
- `MyGUI_widgets` (`examples/widgets.cpp`): every built-in control inside a scroll view.

`examples/common/Example.hpp` holds their shared font lookup and event loop. Each accepts `--frames N` for smoke tests. The full demo lives in `examples/demo/main.cpp` and builds as `MyGUI`.

The demo provides an overlay-toggle button, a click counter, a disabled button, and clipped overlapping panels. Hover the overlay, resize the window, or press Tab/Shift+Tab to change focus and Enter/Space to activate a button. V toggles the overlay. `--log-events` logs event names, coordinates, actions, modifiers, Unicode code points, and resize sources; `--frames N` exits after N frames for smoke tests. `--width N --height N` selects the starting window size.

The input diagnostics display logical mouse coordinates, accumulated movement and scroll,
held/pressed/released states for the left mouse button and V key, and the last key,
button, or text event. Single-frame edges can be brief; event logging records every
transition even when a press and release happen during the same frame.

## Tests

```sh
ctest --preset debug
```

Renderer and window integration tests require a display. For software rendering on a machine without a display:

```sh
sudo apt-get install xvfb xauth
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --preset debug
```

Input, widget, layout, font, and image tests run without a display:

```sh
ctest --preset debug -L headless
```

CI builds Debug and Release on Ubuntu and runs every suite under Xvfb/Mesa, smoke-runs the demo and examples, and builds `tests/package` against an installed copy. Windows and macOS jobs build everything and run the `headless` label. The input test executable has no GLFW, OpenGL, or FreeType dependency; the other headless suites link the library but never initialize GLFW or create a context.

To build just the library and demo, configure with `-DBUILD_TESTING=OFF`. To save a renderer-test preview, set `MYGUI_RENDER_SNAPSHOT` to a PPM output path when running `MyGUI_tests`.

## Windows, events, and resource lifetimes

All window operations, input feeding, UI dispatch, and drawing belong on the main thread. The library initializes GLFW for the first `gui::Window` and shuts it down after the last one. Destroying or replacing one window leaves other windows usable. Do not mix this ownership with externally managed native GLFW windows, or destroy windows inside native callbacks.

Call `pollEvents()` on **one** live window once per application frame. `waitEvents(timeout)` processes events the same way but sleeps until at least one arrives (a negative timeout waits indefinitely, zero polls, a positive timeout returns after that many seconds). `Window::postEmptyEvent()` wakes a waiting main thread from any thread. GLFW processes events globally, so the call resets pressed/released edges and scroll offsets for every library window, then polls all of them. Held state and cursor positions persist. Movement deltas reset at the same boundary
and accumulate signed logical-coordinate differences across mouse movement, button,
and scroll position samples. The first sample establishes a baseline without movement.
Focus changes and cursor entry/leave invalidate that baseline; the next sample avoids a re-entry
jump while retaining any movement already recorded in the frame. Focus loss clears
held keys/buttons and marks release edges, without manufacturing release events. Callback exceptions are saved and rethrown at the polling boundary, never through GLFW; if several windows have pending exceptions, subsequent polls report the remaining ones. Recursive polling is rejected.

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

### Drawing only when needed

`UIContext::needsRedraw()` reports whether anything visible may have changed since the last `draw()`: layout, hover, focus, handled events, widget property setters, theme changes, and `WindowRefreshEvent`s, which windows emit when the platform needs their contents drawn again (for example after being uncovered). Custom widgets should call the protected `requestRedraw()` from setters that change their appearance; `invalidateLayout()` implies it. An application can wait for events and skip frames while nothing changed, so an idle window uses no CPU:

```cpp
while (!window.shouldClose()) {
    ui.layout(window.viewport());
    if (ui.needsRedraw()) {
        renderer.beginFrame(window.viewport());
        renderer.clear(ui.theme().background);
        ui.draw(renderer);
        renderer.endFrame();
        window.swapBuffers();
    }
    window.waitEvents();
    while (auto event = window.nextEvent()) ui.handleEvent(*event);
}
```

Animations need a frame source: use `waitEvents(timeout)` or call `ui.requestRedraw()`. Frames drawn without `ui.draw()` can clear the flag with `markDrawn()`. The demo's input diagnostics update only when events arrive.

Call `window.makeContextCurrent()` before creating, drawing with, or destroying its renderer. `setSwapInterval(1)` makes that window current and enables vsync. Use one renderer per context and declare it after its window so GPU resources are destroyed first. Font objects contain CPU resources and can be shared by renderers; glyph atlases remain owned by each renderer. Window resize callbacks update cached size/event data without changing any OpenGL viewport.
`WindowResizeEvent::source` distinguishes `ResizeSource::Window` from
`ResizeSource::Framebuffer`; both retain `EventType::WindowResized` and contain the
latest logical and framebuffer dimensions. Framebuffer callbacks also query the native
logical dimensions because some backends can omit a logical-size callback when a
hidden window is resized. `viewport()` returns the same cached snapshot; `Renderer::beginFrame()` sets the viewport.

The renderer manages its drawing state rather than restoring another graphics engine's bindings. Frames use `beginFrame`, `clear`, draw calls, and `endFrame`. Besides `drawRect` and `drawText`, `drawRoundedRect` and `drawBorder` render anti-aliased rounded shapes (radii are clamped to half the shorter side; borders are drawn inside their rectangle), and `drawImage` stretches a `gui::Image` over a rectangle, multiplied by a tint. Clearing discards earlier queued drawing and clears the whole framebuffer, regardless of the active clip. Clip pushes intersect their parents; pops must balance before `endFrame`. A new frame resets the clip stack. Drawing uses logical coordinates, while scissor rectangles and glyph rasterization account for independent framebuffer X/Y scales.

## Images

`gui::Image` holds immutable straight-alpha RGBA8 pixels with the top row first. `Image::load(path)` and `Image::decode(bytes)` read PNG and JPEG through a vendored copy of [stb_image](third_party/stb/SOURCE.md), compiled privately into the library; `Image::fromPixels(width, height, rgba)` wraps pixels you generate. Dimensions are limited to 16384. Like fonts, images are CPU resources that can be shared by renderers. Each renderer uploads an image on first use and keeps its texture until `releaseImage()` or the renderer's destruction; releasing an image still queued in the current frame is rejected.

`gui::ImageView` displays an image with `ImageScaling::Fit` (the default, keeps the aspect ratio), `Stretch`, or `Center` (natural size, clipped to the widget). Its preferred size is the image size.

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

Buttons activate on a matching left-button release inside the visible hit area, or on a matching Enter/Space release while focused. Dragging outside, focus loss, or disabling cancels activation. Keyboard repeats do not activate. Subclasses can override the protected `activate()` hook to reuse these rules.

## Controls and themes

- `gui::Checkbox` follows the button activation rules: clicking anywhere on it, or Enter/Space while focused, toggles `checked()` and calls `setOnChange` before any click callback. `gui::Switch` is the same control drawn as a sliding pill. `setChecked` never notifies.
- `gui::Slider` has a finite range, an optional step (`0` is continuous), and `setOnChange` for user changes. Pressing captures the pointer, so dragging keeps tracking the cursor outside the widget until release. Arrow keys step (repeats included; continuous sliders step by 1% of the range), PageUp/PageDown move ten steps, and Home/End jump to the ends. `setValue` clamps, snaps and ignores NaN without notifying.
- `gui::ProgressBar` shows a value clamped to `[0, 1]` and takes no input.

## Text entry

`gui::TextField` edits a single line of UTF-8. Typing arrives as `TextInputEvent`s, so keyboard layouts and dead keys work as the platform delivers them; control characters are never inserted and `setText` strips newlines and replaces invalid UTF-8 with U+FFFD. The caret is a byte offset that always sits on a code point boundary: Left/Right move by code point, Home/End jump to the ends, Backspace/Delete remove one code point, and a click places the caret at the nearest boundary. Long text scrolls horizontally to keep the caret visible. `setOnChange` runs after every user edit (never for `setText`), and `setOnSubmit` runs when Enter is pressed. While focused, the field consumes key events without Ctrl/Alt/Super so typing cannot trigger application shortcuts, plus the editing shortcuts below; Escape, Up, Down, PageUp, PageDown and other modifier shortcuts still bubble. The caret does not blink.

The selection runs from an anchor to the caret. Shift with Left/Right/Home/End or a click extends it, dragging selects with pointer capture, and Ctrl+A selects everything. Ctrl+Left/Right move by word and Ctrl+Backspace/Delete delete by word; words are separated by spaces. Typing, Backspace and Delete replace the selection. Ctrl+C, Ctrl+X and Ctrl+V (or Super on macOS) copy, cut and paste through the `gui::Clipboard` set with `ui.setClipboard(&window.clipboard())`; pasted text is cleaned like `setText`, and without a clipboard these shortcuts do nothing. `Clipboard` is a small interface, so tests and other backends can supply their own. On Wayland, the compositor only lets a window that has received input own the clipboard.

`Font::caretOffsets` returns the x offset of every code point boundary, and `appendUtf8`, `sanitizeUtf8`, `nextUtf8Boundary` and `previousUtf8Boundary` are available for custom text widgets.

## Scrolling

`gui::ScrollView` scrolls a single `content()` container (vertical layout by default); add children to the content, not to the view. The content receives at least the viewport size and grows to its preferred size. Vertical scrolling is always available; `setHorizontalScrolling(true)` enables the horizontal axis. Scrollbars appear only when needed and occupy a gutter that content never covers, for drawing or hit-testing.

The wheel scrolls the innermost view under the cursor that can still move in that direction, then bubbles to outer views. Dragging a thumb captures the pointer; clicking the track pages. PageUp/PageDown bubbling unhandled from a focused descendant scroll by one viewport. Tab traversal calls `UIContext::reveal`, which scrolls every enclosing view so the newly focused widget is visible; call it yourself after programmatic focus changes. `setScrollOffset` clamps against the latest layout. Custom containers can implement the virtual `childClip()` and `scrollIntoView()` hooks.

`gui::Theme` holds the palette, corner radius and focus-ring width the built-in widgets read while drawing. Set it with `ui.setTheme(theme)`; detached widgets use the default theme. Buttons derive their colors from the theme unless `setStyle(ButtonStyle)` overrides them; `resetStyle()` returns to the theme.

## Text support and current limits

Fonts are loaded through FreeType. Sizes are logical pixels in `(0, 4096]`, with scaled raster sizes capped at 4096 pixels. `measureText()` returns advance width, line height, ascent, and descent; measurement is independent of framebuffer scale. UTF-8 text supports explicit newlines and four-space tabs. Invalid UTF-8 bytes become U+FFFD; unsupported characters use U+FFFD, `?`, or the font's missing-glyph entry. Font loading and invalid sizes report exceptions.

`label.setWrap(true)` breaks lines at spaces (long words split between code points) to fit the label's width, using `Font::wrapText`. Wrapped labels report their height through the virtual `Widget::heightForWidth`: vertical layouts and scroll views ask each child for its height at the final width, and containers answer for their children. A wrapped label's preferred width remains its unwrapped width, so horizontal layouts can shrink it toward its minimum. Center and right alignment apply to each line.

Glyph coverage textures are cached per renderer, font, and raster size. Atlas pages remain fixed while queued commands reference them. The cache lasts for the renderer's lifetime; applications displaying many different font sizes should bound their font-size choices.

This version does not include multi-line text editing, complex-script shaping, bidirectional layout, color emoji, or native accessibility integration. Labels and buttons clip text that exceeds their bounds. The bundled Noto Sans font's pinned source and SIL Open Font License are in `assets/fonts/SOURCE.md` and `assets/fonts/OFL.txt`.
