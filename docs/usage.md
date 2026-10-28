# MyGUI usage reference

See the [README](../README.md) for setup, examples, and CMake integration.
The [demo source](../examples/demo/main.cpp) shows a complete application.

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

`gui::Image` holds immutable straight-alpha RGBA8 pixels with the top row first. `Image::load(path)` and `Image::decode(bytes)` read PNG and JPEG through a vendored copy of [stb_image](../third_party/stb/SOURCE.md), compiled privately into the library; `Image::fromPixels(width, height, rgba)` wraps pixels you generate. Dimensions are limited to 16384. Like fonts, images are CPU resources that can be shared by renderers. Each renderer uploads an image on first use and keeps its texture until `releaseImage()` or the renderer's destruction; releasing an image still queued in the current frame is rejected.

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

This version does not include multi-line text editing, complex-script shaping, bidirectional layout, color emoji, or native accessibility integration. Labels and buttons clip text that exceeds their bounds. The bundled Noto Sans font's pinned source and SIL Open Font License are in [font source](../assets/fonts/SOURCE.md) and [license](../assets/fonts/OFL.txt).

## Test details

CI builds Debug and Release on Ubuntu, runs all tests under Xvfb/Mesa, smoke-tests
the examples, and builds an installed-package consumer. Windows and macOS CI
build everything and run headless tests.
The input test executable has no GLFW, OpenGL, or FreeType dependency. The other
headless suites link the library but never initialize GLFW or create a context.

To build the library, demo, and examples, disable tests when configuring:

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

For redraw-on-demand/native capture diagnostics, see the
[presentation investigation](presentation-investigation.md) and its opt-in tools.

## Overlays and modal dialogs

`UIContext` owns overlays independently of the ordinary root. Overlay roots fill
its logical viewport; absolute child bounds use window coordinates. Overlays draw
after ordinary content and escape its clips, but remain clipped to the viewport.
Nonmodal overlays draw in opening order, below modal overlays; hit testing uses
the reverse order. `openOverlay(std::unique_ptr<Container>, OverlayOptions)`
accepts ordinary widget trees (`modal` defaults to false).

```cpp
#include <gui/Dialog.hpp>
#include <gui/TextField.hpp>

auto dialog = std::make_unique<gui::Dialog>("Settings", font);
auto& field = dialog->content().emplace<gui::TextField>(font, "My name");
dialog->addAction("save", "Save");
dialog->addCancelAction();
auto handle = ui.openDialog(std::move(dialog),
    [&, input = &field](gui::Dialog&, const gui::DialogResult& result) {
        status.setText(result.action == "save" ? "Saved: " + input->text()
                                              : "Cancelled");
    });
// Programmatic cancellation, safe even if already closed:
// ui.closeOverlay(handle);
```

`content()` is a vertical container inside a scroll view. `actions()` accepts
existing buttons and containers; `addAction(id, label)` creates a button that
closes with that identifier. `addCancelAction(label)` closes with no action.
For custom actions, call `Dialog::close({actionId})` or `close()` to cancel.
Configure initial content and actions before transferring ownership. References
follow normal widget lifetime rules; do not retain them after completion.

Dialog sheets center and resize with the viewport. Large content scrolls while
normal-size title and action areas remain outside the body scroll view. Actions
stack vertically when their preferred widths cannot fit. At very small sizes,
the whole sheet scrolls as well, keeping its actions reachable. Theme fields
`dialogBackdrop`, `dialogPadding`, `dialogSpacing`, and `dialogMargin` supplement
the existing surface, border, text, corner-radius, and control focus styling.

`setDismissOnEscape(false)` disables the default Escape cancellation.
`setDismissOnBackdropClick(true)` enables cancellation on a left-button press
outside the sheet; this is disabled by default. Escape is applied only after
widgets decline the event. Enter retains the focused control's behavior: a
text field submits without activating another action. Tab and Shift+Tab are
reserved for traversal within the top visible modal, skipping hidden, disabled,
and non-focusable descendants. A childless modal still blocks background input.

Opening a modal cancels background hover, presses, and capture, and saves the
previous focus. Closing restores it only when still attached, visible, enabled,
focusable, and allowed by the remaining modal. Otherwise the first eligible
widget in tree order receives focus, or focus clears. Opening another dialog
stacks it above the first; closing it returns control to the lower dialog.
Closing a lower dialog leaves the upper dialog active. Hidden modal roots
suspend their boundary; disabled visible modal roots continue to block input.
Window focus loss cancels interaction; regaining focus enters the active modal.

Blocked pointer, wheel, key, and text events return handled, so application
shortcuts following `handleEvent()` do not run. Window lifecycle events remain
available to the application. Buttons and keys held across a boundary change
have their remaining repeats/releases swallowed, preventing dismissal click-through.
Consuming input solely at the modal boundary does not request redraw. Settled
dialogs need no continuous frame source.

`OverlayHandle` is copyable and non-owning; dropping it does not close anything.
`closeOverlay(handle)` returns true only for the first accepted closure.
Default, stale, foreign-context, and already-closed handles return false;
`isOverlayOpen(handle)` queries the same ownership identity safely.

Completion receives a `DialogResult` whose optional `action` identifies the
selected action; an empty value means cancellation, including programmatic,
Escape, backdrop, and Cancel-button closure. During dispatch, closure cancels
interaction immediately, then waits until the event route has unwound before
running completion. The completed dialog and its remaining children stay alive
through that callback, so reading the text field above is safe. Completion may
open or close other dialogs. It runs exactly once, including if it throws;
exceptions propagate after lifecycle cleanup. Destroying a `UIContext` discards
open dialogs and callbacks **without invoking application callbacks**.

Try `MyGUI_dialogs` for confirmation, editable settings, nested dialogs, and
background controls that remain inactive while a modal is open.

## Anchored popups and ComboBox

`gui::ComboBox` is a non-editable selection control. Identifiers are unique opaque
strings; labels can repeat. Disabled items cannot be selected. New controls start
unselected and display their placeholder.

```cpp
#include <gui/ComboBox.hpp>
auto& choice = ui.root().emplace<gui::ComboBox>(font);
choice.setItems({{"fast", "Fast"}, {"slow", "Slow"}, {"off", "Unavailable", false}});
choice.setPlaceholder("Choose a mode…");
choice.setSelectedId("fast"); // Programmatic: no notification.
choice.setOnChange([&](const gui::ComboBoxItem& item) {
    status.setText("Selected: " + item.label);
});
```

`selectedId()` returns an optional string value; `setSelectedId(std::nullopt)`
clears it. Unknown or disabled identifiers return false without changing state.
Valid programmatic selection requests close the popup silently. `setItems()`
closes the popup and preserves selection only if its identifier still names an
enabled item. Duplicate identifiers throw `std::invalid_argument` before changing
anything. Empty and all-disabled lists never open. Labels are sanitized to valid
UTF-8 using the existing font helpers.

Click, Enter, Space, or Down opens the list. The opening gesture cannot commit.
Up/Down skip disabled items and stop at the ends; Home/End highlight the first/last
enabled item. Hover changes only the highlight. Enter or a completed click on an
enabled row commits; committing a different identifier notifies once, while
committing the same identifier closes silently. Escape and outside presses
cancel. Tab/Shift+Tab cancel and traverse focus once. Logical focus remains on the
ComboBox; rows are not Tab stops. Keyboard navigation scrolls the candidate into
view. Wheel events over the list never scroll content underneath, even at a limit.

Type-ahead works only while open. Printable `TextInputEvent`s accumulate a prefix
with a 750 ms timeout. Matching chooses the first enabled label prefix in item
order, ignores ASCII case, and compares other Unicode code points exactly (no
Unicode normalization or case folding). A failed match keeps the highlight.
Repeated letters extend the prefix rather than cycling. The timeout is checked
on the next text event and never requires continuous redraw. The constructor's
optional third `TimeSource` argument supplies a `steady_clock::time_point` getter
for deterministic testing; ordinary applications use the default steady clock.

For reusable hosting, construct `gui::Popup`, populate its `content()` container,
and call `ui.openPopup(owner, std::move(popup), PopupOptions{})`. The context owns
it and returns the existing context-specific `OverlayHandle`. `maxHeight` defaults
to 240 logical units and `gap` to 2. Roots are placed below their owner when they
fit, or above when that provides more room. Width is at least the owner's width
and expands toward content preference, capped to the viewport. Height is capped
by available space and `maxHeight`; overflow uses the existing ScrollView.
Placement updates after layout, resizing, and ancestor scrolling, outside ancestor
clips but within the viewport. Nonpositive usable area prevents opening.

One transient popup is active per context; another eligible opening replaces it.
An unavailable, foreign-context, fully clipped, or modal-blocked owner returns an
empty handle without replacing an existing popup. Popup descendants cannot own
another popup. Popups close when their owner becomes unavailable or fully clipped,
on focus loss, or when focus moves away from the owner. Generic hosts preserve
logical focus; ComboBox explicitly focuses itself. Popup children receive pointer
input and capture without taking keyboard focus. Handle closure is idempotent.
There is no popup completion callback and no teardown notification.

A popup inside a modal draws above that dialog and shares its interaction boundary.
Opening a newer modal closes the popup in the blocked scope. Escape closes only
the popup; its repeats/release are consumed, so another Escape is needed to dismiss
the dialog. Outside presses are consumed with their matching releases; a fresh
click works normally. Keyboard/text input while the list is open cannot fall
through to application shortcuts. Wheel input outside the popup may reach other
permitted content, repositioning or closing the popup as its anchor moves.

Callbacks receive a copied item payload valid through the call. Selection and
closure finish before notification, allowing callbacks to remove the control or
its parent, replace items, or open a dialog or another popup. Do not retain item
references beyond the callback or widget references after removal. Copies of a
ComboBox retain configuration and committed selection, never an open popup or
preview state. `MyGUI_combobox` demonstrates long lists, disabled options, modal
ownership, and edge placement.
