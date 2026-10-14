// These tests never create a window or an OpenGL context.
#include "gui/Button.hpp"
#include "gui/Checkbox.hpp"
#include "gui/ProgressBar.hpp"
#include "gui/ScrollView.hpp"
#include "gui/Slider.hpp"
#include "gui/TextField.hpp"
#include "gui/UIContext.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int failures = 0;
void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++failures; }
bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }
void move(gui::UIContext& ui, double x, double y) { ui.handleEvent(gui::Event{gui::MouseMoveEvent{x, y}}); }
void mouse(gui::UIContext& ui, double x, double y, gui::ButtonAction action) {
    ui.handleEvent(gui::Event{gui::MouseButtonEvent{gui::MouseButton::Left, action, x, y}});
}
void click(gui::UIContext& ui, double x, double y) {
    mouse(ui, x, y, gui::ButtonAction::Press); mouse(ui, x, y, gui::ButtonAction::Release);
}
void wheel(gui::UIContext& ui, double x, double y, double dy, double dx = 0) {
    ui.handleEvent(gui::Event{gui::MouseScrollEvent{dx, dy, x, y}});
}
void key(gui::UIContext& ui, gui::Key key, gui::KeyAction action, bool shift = false) {
    ui.handleEvent(gui::Event{gui::KeyEvent{key, 0, action, {.shift = shift}}});
}
bool press(gui::UIContext& ui, gui::Key key, gui::Modifiers mods = {}) {
    return ui.handleEvent(gui::Event{gui::KeyEvent{key, 0, gui::KeyAction::Press, mods}});
}
void type(gui::UIContext& ui, std::u32string_view text) {
    for (const auto codepoint : text) ui.handleEvent(gui::Event{gui::TextInputEvent{codepoint}});
}
void testLayout() {
    gui::UIContext ui;
    auto& root = ui.root();
    root.setLayout(gui::Layout::Horizontal); root.setPadding({10, 10, 10, 10}); root.setSpacing(10);
    auto& a = root.emplace<gui::Panel>(); a.setPreferredSize({100, 20}); a.setMinimumSize({40, 10}); a.setFlex(1);
    auto& b = root.emplace<gui::Panel>(); b.setPreferredSize({100, 20}); b.setMinimumSize({40, 10}); b.setFlex(3);
    ui.layout({430, 100, 645, 150});
    check(near(a.bounds().x, 10) && near(a.bounds().width, 150) && near(b.bounds().x, 170)
          && near(b.bounds().width, 250) && near(b.bounds().height, 80), "horizontal flex, padding, spacing use logical units");
    ui.layout({150, 100, 150, 100});
    check(near(a.bounds().width, 60) && near(b.bounds().width, 60), "preferred sizes shrink proportionally toward minimums");
    ui.layout({30, 10, 30, 10});
    check(near(a.bounds().width, 40) && near(b.bounds().width, 40) && a.bounds().height >= 0,
          "undersized parent retains child minimums and never produces negative bounds");
    b.setVisible(false); ui.layout({150, 100, 150, 100});
    check(near(a.bounds().width, 130), "hidden children reserve neither space nor spacing");
    b.setVisible(true); b.setEnabled(false); ui.layout({150, 100, 150, 100});
    check(near(b.bounds().width, 60), "disabled children retain layout space");
    root.setLayout(gui::Layout::Vertical); a.setPreferredSize({10, 20}); b.setPreferredSize({10, 20});
    a.setFlex(0); b.setFlex(1); ui.layout({150, 150, 150, 150});
    check(near(a.bounds().height, 20) && near(b.bounds().y, 40) && near(b.bounds().height, 100), "vertical layout expands flex children");
    a.setBounds({0, 0, -10, -20}); check(a.bounds().empty(), "negative explicit dimensions are normalized");
}
void testButtonsAndFocus() {
    gui::UIContext ui; ui.layout({400, 200, 400, 200});
    auto& first = ui.root().emplace<gui::Button>("first"); first.setBounds({10, 10, 100, 40});
    auto& second = ui.root().emplace<gui::Button>("second"); second.setBounds({130, 10, 100, 40});
    auto& disabled = ui.root().emplace<gui::Button>("disabled"); disabled.setBounds({250, 10, 100, 40}); disabled.setEnabled(false);
    int clicks = 0; first.setOnClick([&] { ++clicks; });
    move(ui, 20, 20); check(first.hovered(), "hover is managed by the UI context");
    mouse(ui, 20, 20, gui::ButtonAction::Press);
    check(first.pressed() && first.focused() && ui.capturedWidget() == &first, "press focuses and captures the button");
    move(ui, 120, 100); check(!first.pressed(), "dragging outside clears pressed appearance");
    mouse(ui, 120, 100, gui::ButtonAction::Release);
    check(clicks == 0 && !ui.capturedWidget(), "outside release cancels activation and releases capture");
    click(ui, 20, 20); check(clicks == 1, "inside press and release activate exactly once");
    mouse(ui, 20, 20, gui::ButtonAction::Release); check(clicks == 1, "unmatched release does not activate");
    key(ui, gui::Key::Space, gui::KeyAction::Press);
    key(ui, gui::Key::Space, gui::KeyAction::Repeat); key(ui, gui::Key::Space, gui::KeyAction::Repeat);
    check(clicks == 1 && first.pressed(), "key repeats never activate the button");
    key(ui, gui::Key::Space, gui::KeyAction::Release);
    key(ui, gui::Key::Space, gui::KeyAction::Release); check(clicks == 2, "matching key release activates once");
    key(ui, gui::Key::Tab, gui::KeyAction::Press); check(second.focused(), "Tab advances focus and skips disabled controls");
    key(ui, gui::Key::Tab, gui::KeyAction::Press); check(first.focused(), "focus traversal wraps");
    key(ui, gui::Key::Tab, gui::KeyAction::Press, true); check(second.focused(), "Shift+Tab traverses backward");
    second.setVisible(false); check(!ui.focusedWidget(), "hiding a focused widget clears focus");
    ui.requestFocus(&first); key(ui, gui::Key::Enter, gui::KeyAction::Press);
    ui.handleEvent(gui::Event{gui::WindowFocusEvent{false}});
    check(!first.pressed() && !ui.focusedWidget() && !ui.capturedWidget() && !first.hovered(), "window focus loss cancels interactions");
    ui.handleEvent(gui::Event{gui::WindowFocusEvent{true}});
    key(ui, gui::Key::Enter, gui::KeyAction::Release); check(clicks == 2, "focus regain cannot complete a stale key press");
    mouse(ui, 20, 20, gui::ButtonAction::Press); first.setEnabled(false);
    check(!ui.capturedWidget() && !first.pressed() && !first.focused(), "disabling a pressed control clears focus and capture");
    mouse(ui, 20, 20, gui::ButtonAction::Release); check(clicks == 2, "disabled button cannot activate");
    first.setEnabled(true); move(ui, 20, 20);
    ui.handleEvent(gui::Event{gui::CursorEnterEvent{false}}); check(!first.hovered(), "cursor leave clears hover");
    ui.handleEvent(gui::Event{gui::CursorEnterEvent{true}}); check(first.hovered(), "cursor entry restores hover");
    click(ui, 110, 20); check(clicks == 2, "hit-test right boundary is exclusive");
    mouse(ui, 20, 20, gui::ButtonAction::Press);
    key(ui, gui::Key::Enter, gui::KeyAction::Press); key(ui, gui::Key::Enter, gui::KeyAction::Release);
    mouse(ui, 20, 20, gui::ButtonAction::Release);
    check(clicks == 3 && !ui.capturedWidget(), "switching from pointer to keyboard activation cancels the stale pointer press");
}
struct BubblingContainer : gui::Container {
    int* visits;
    explicit BubblingContainer(int& count) : visits(&count) {}
    bool onEvent(const gui::Event&, gui::UIContext&) override { ++*visits; return true; }
};
void testRoutingAndClips() {
    gui::UIContext ui; ui.layout({300, 200, 600, 300});
    auto& underneath = ui.root().emplace<gui::Button>("underneath"); underneath.setBounds({20, 20, 160, 100});
    int clicks = 0; underneath.setOnClick([&] { ++clicks; });
    int visits = 0;
    auto& top = ui.root().emplace<BubblingContainer>(visits); top.setBounds({40, 40, 80, 50});
    auto& passive = top.emplace<gui::Panel>(gui::Rect{40, 40, 160, 100});
    click(ui, 50, 50); check(visits == 2 && clicks == 0, "unconsumed child events bubble to topmost ancestor");
    click(ui, 130, 50); check(clicks == 1, "content outside ancestor clip does not intercept input");
    top.setEnabled(false); click(ui, 50, 50); check(clicks == 1, "disabled topmost widget blocks underlying controls");
    top.setEnabled(true); top.setClipChildren(false); click(ui, 130, 50);
    check(clicks == 1 && visits == 4, "unclipped children can receive input outside parent bounds");
    top.setEnabled(false); click(ui, 130, 50);
    check(clicks == 1 && visits == 4, "disabled ancestors still block input through unclipped visible descendants");
    top.setEnabled(true); top.setClipChildren(true);
    auto& nested = top.emplace<gui::Container>(gui::Rect{40, 40, 40, 50});
    auto& nestedButton = nested.emplace<gui::Button>("nested"); nestedButton.setBounds({40, 40, 100, 40});
    int nestedClicks = 0; nestedButton.setOnClick([&] { ++nestedClicks; });
    click(ui, 60, 50); check(nestedClicks == 1, "nested target is clickable within all ancestor clips");
    mouse(ui, 60, 50, gui::ButtonAction::Press); mouse(ui, 90, 50, gui::ButtonAction::Release);
    check(nestedClicks == 1 && !ui.capturedWidget(), "release outside ancestor clip cancels captured click");
    top.setVisible(false); check(!ui.focusedWidget(), "hiding an ancestor clears descendant focus");
    (void)passive;
}
void testTreeMutations() {
    gui::UIContext ui; ui.layout({300, 200, 300, 200});
    auto& button = ui.root().emplace<gui::Button>("remove me"); button.setBounds({10, 10, 100, 40});
    bool deferred = false;
    button.setOnClick([&] {
        ui.root().remove(button);
        deferred = ui.root().children().size() == 1 && !ui.focusedWidget();
        auto& added = ui.root().emplace<gui::Button>("new"); added.setBounds({10, 10, 100, 40});
    });
    click(ui, 20, 20);
    check(deferred && ui.root().children().size() == 1 && !ui.capturedWidget(), "self-removal and addition commit after dispatch");
    auto& group = ui.root().emplace<gui::Container>(gui::Rect{150, 10, 120, 100});
    auto& child = group.emplace<gui::Button>("remove group"); child.setBounds({150, 10, 100, 40});
    child.setOnClick([&] { ui.root().remove(group); group.remove(child); });
    click(ui, 160, 20); check(ui.root().children().size() == 1, "removing a parent and its active child is safe");
    auto& throwing = ui.root().emplace<gui::Button>("throw"); throwing.setBounds({150, 10, 100, 40});
    throwing.setOnClick([&] { ui.root().remove(throwing); throw std::runtime_error("handler failure"); });
    bool caught = false;
    try { click(ui, 160, 20); } catch (const std::runtime_error&) { caught = true; }
    check(caught && ui.root().children().size() == 1 && !ui.dispatching(), "throwing callbacks still commit removal and reset dispatch guard");
    click(ui, 20, 20); check(!ui.dispatching(), "UI remains usable after a handler exception");
    auto& copySource = static_cast<gui::Button&>(*ui.root().children().front());
    move(ui, 20, 20);
    ui.requestFocus(&copySource); key(ui, gui::Key::Space, gui::KeyAction::Press);
    gui::Button copied = copySource;
    check(!copied.parent() && !copied.hovered() && !copied.focused() && !copied.pressed(), "widget copies never copy attachment or interaction state");
}
void testThemeAndCheckbox() {
    gui::UIContext ui; ui.layout({400, 200, 400, 200});
    auto& box = ui.root().emplace<gui::Checkbox>("Box"); box.setBounds({10, 10, 120, 24});
    auto& button = ui.root().emplace<gui::Button>("Button"); button.setBounds({10, 60, 100, 40});
    gui::Button detached{"detached"};
    check(detached.style().normal.r == gui::Theme{}.accent.r, "detached widgets fall back to the default theme");
    gui::Theme theme; theme.accent = {0.5f, 0.1f, 0.1f, 1}; theme.cornerRadius = 2;
    ui.setTheme(theme);
    check(button.style().normal.r == 0.5f && button.style().cornerRadius == 2, "button style follows the UI theme");
    gui::ButtonStyle custom; custom.normal = {0, 0, 1, 1};
    button.setStyle(custom); ui.setTheme(gui::Theme{});
    check(button.style().normal.b == 1, "a custom button style overrides the theme");
    button.resetStyle(); check(button.style().normal.r == gui::Theme{}.accent.r, "resetStyle returns to the theme");

    std::vector<bool> changes; int clicks = 0;
    box.setOnChange([&](bool checked) { changes.push_back(checked); });
    box.setOnClick([&] { ++clicks; });
    click(ui, 100, 20);
    check(box.checked() && changes == std::vector<bool>{true} && clicks == 1, "clicking the label toggles and notifies once");
    key(ui, gui::Key::Space, gui::KeyAction::Press); key(ui, gui::Key::Space, gui::KeyAction::Repeat);
    key(ui, gui::Key::Space, gui::KeyAction::Release);
    check(!box.checked() && changes.size() == 2, "Space toggles on release and ignores repeats");
    mouse(ui, 20, 20, gui::ButtonAction::Press); move(ui, 300, 150); mouse(ui, 300, 150, gui::ButtonAction::Release);
    check(!box.checked() && changes.size() == 2, "releasing outside cancels the toggle");
    box.setChecked(true); check(box.checked() && changes.size() == 2, "programmatic changes do not notify");
    box.setEnabled(false); click(ui, 20, 20); check(box.checked(), "disabled checkboxes ignore clicks");
    gui::Switch toggle{"Switch", nullptr, true};
    check(toggle.checked() && toggle.preferredSize().width > gui::Checkbox{}.preferredSize().width,
          "switches are checkboxes with a wider indicator");
}
void testSlider() {
    gui::UIContext ui; ui.layout({400, 200, 400, 200});
    auto& slider = ui.root().emplace<gui::Slider>(0.0f, 100.0f, 50.0f); slider.setBounds({10, 10, 216, 24});
    std::vector<float> values;
    slider.setOnChange([&](float value) { values.push_back(value); });
    // The track runs from x = 18 to x = 218, so each logical unit is half a value unit.
    mouse(ui, 18, 20, gui::ButtonAction::Press);
    check(slider.dragging() && ui.capturedWidget() == &slider && slider.value() == 0, "press captures and jumps to the cursor");
    move(ui, 118, 190); check(near(slider.value(), 50), "captured drags follow x outside the bounds");
    move(ui, 1000, 20); check(slider.value() == 100, "drags clamp to the maximum");
    mouse(ui, -50, 20, gui::ButtonAction::Release);
    check(!slider.dragging() && !ui.capturedWidget() && slider.value() == 0, "release ends the drag at the final position");
    check(values.size() == 4, "each changed value notifies once");
    slider.setStep(10); slider.setValue(44); check(slider.value() == 40, "values snap to the step");
    slider.setValue(std::nanf("")); check(slider.value() == 40, "NaN values are ignored");
    ui.requestFocus(&slider); values.clear();
    key(ui, gui::Key::Right, gui::KeyAction::Press); key(ui, gui::Key::Right, gui::KeyAction::Repeat);
    check(slider.value() == 60 && values.size() == 2, "arrow keys step, including repeats");
    key(ui, gui::Key::PageDown, gui::KeyAction::Press); check(slider.value() == 0, "PageDown moves ten steps and clamps");
    key(ui, gui::Key::End, gui::KeyAction::Press); check(slider.value() == 100, "End jumps to the maximum");
    key(ui, gui::Key::Up, gui::KeyAction::Press); check(values.size() == 4, "unchanged values do not notify");
    slider.setRange(0, 30); check(slider.value() == 30, "narrowing the range clamps the value");
    bool caught = false;
    try { slider.setRange(5, 1); } catch (const std::invalid_argument&) { caught = true; }
    check(caught, "inverted ranges are rejected");
    caught = false;
    try { slider.setStep(-1); } catch (const std::invalid_argument&) { caught = true; }
    check(caught, "negative steps are rejected");
    mouse(ui, 100, 20, gui::ButtonAction::Press); slider.setEnabled(false);
    check(!slider.dragging() && !ui.capturedWidget(), "disabling cancels a drag");
    mouse(ui, 50, 20, gui::ButtonAction::Press); check(!slider.dragging(), "disabled sliders ignore presses");
    slider.setEnabled(true); mouse(ui, 100, 20, gui::ButtonAction::Press);
    gui::Slider copy = slider; check(!copy.dragging() && copy.value() == slider.value(), "copies keep the value but not the drag");
    gui::ProgressBar progress{1.5f}; check(progress.value() == 1, "progress clamps to one");
    progress.setValue(std::nanf("")); check(progress.value() == 0, "NaN progress becomes zero");
}
void testScrollView() {
    gui::UIContext ui;
    auto& view = ui.root().emplace<gui::ScrollView>(); view.setBounds({0, 0, 300, 200});
    std::vector<gui::Button*> rows; int clicked = -1;
    for (int i = 0; i < 10; ++i) {
        // Buttons add 16 units of padding to the preferred height: rows are 50 tall.
        auto& row = view.content().emplace<gui::Button>("row"); row.setPreferredSize({0, 34});
        row.setOnClick([&clicked, i] { clicked = i; }); rows.push_back(&row);
    }
    ui.layout({300, 200, 300, 200});
    check(near(view.contentSize().height, 500) && near(view.maxScrollOffset().y, 300), "content keeps its preferred height");
    check(near(view.viewport().width, 290) && near(rows[0]->bounds().width, 290), "the scrollbar gutter is never covered by content");
    check(near(view.maxScrollOffset().x, 0), "horizontal scrolling is off by default");
    wheel(ui, 50, 50, -1);
    check(near(view.scrollOffset().y, gui::ScrollView::lineStep) && near(rows[0]->bounds().y, -gui::ScrollView::lineStep), "wheel down scrolls content up");
    view.setScrollOffset({0, 1000}); ui.layout({300, 200, 300, 200});
    check(near(view.scrollOffset().y, 300) && near(rows[9]->bounds().y, 150), "offsets clamp to the content end");
    click(ui, 50, 10); check(clicked == 6, "clicks hit the scrolled content");
    wheel(ui, 50, 50, -1); check(near(view.scrollOffset().y, 300), "wheel at the limit leaves the offset alone");
    view.setScrollOffset({0, 0}); ui.layout({300, 200, 300, 200});
    clicked = -1; click(ui, 295, 190);
    check(clicked == -1, "the gutter does not route to content beside it");
    check(near(view.scrollOffset().y, 200), "clicking the track below the thumb pages down");

    // Thumb drag captures and maps thumb travel to the offset range.
    view.setScrollOffset({0, 0}); ui.layout({300, 200, 300, 200});
    // Thumb length is 200 * 200 / 500 = 80, so 120 units of travel cover 300 offset units.
    mouse(ui, 295, 10, gui::ButtonAction::Press); check(view.dragging() && ui.capturedWidget() == &view, "pressing the thumb captures the pointer");
    move(ui, 600, 70); check(near(view.scrollOffset().y, 150), "thumb drag follows the pointer outside the view");
    mouse(ui, 600, 70, gui::ButtonAction::Release); check(!view.dragging() && !ui.capturedWidget(), "release ends the thumb drag");

    view.setScrollOffset({0, 0}); ui.layout({300, 200, 300, 200});
    ui.requestFocus(rows[0]);
    for (int i = 0; i < 9; ++i) key(ui, gui::Key::Tab, gui::KeyAction::Press);
    check(rows[9]->focused() && rows[9]->bounds().y + 50 <= 200 && rows[9]->bounds().y >= 0, "Tab scrolls the focused widget into view");
    key(ui, gui::Key::Tab, gui::KeyAction::Press);
    check(rows[0]->focused() && rows[0]->bounds().y >= 0, "wrapping focus scrolls back to the top");
    key(ui, gui::Key::PageDown, gui::KeyAction::Press);
    check(near(view.scrollOffset().y, 200), "PageDown bubbles from a focused child and scrolls a page");

    // Nested views: the inner one consumes the wheel until it reaches its limit.
    gui::UIContext nested;
    auto& outer = nested.root().emplace<gui::ScrollView>(); outer.setBounds({0, 0, 200, 200});
    auto& inner = outer.content().emplace<gui::ScrollView>(); inner.setPreferredSize({0, 100}); inner.setMinimumSize({0, 100});
    auto& tall = inner.content().emplace<gui::Panel>(); tall.setPreferredSize({0, 300});
    auto& filler = outer.content().emplace<gui::Panel>(); filler.setPreferredSize({0, 400});
    nested.layout({200, 200, 200, 200});
    wheel(nested, 50, 50, -1);
    check(near(inner.scrollOffset().y, 40) && near(outer.scrollOffset().y, 0), "the innermost view consumes the wheel");
    wheel(nested, 50, 50, 1); wheel(nested, 50, 50, 1);
    check(near(inner.scrollOffset().y, 0) && near(outer.scrollOffset().y, 0), "neither view scrolls past its start");
    for (int i = 0; i < 5; ++i) wheel(nested, 50, 50, -1);
    check(near(inner.scrollOffset().y, 200) && near(outer.scrollOffset().y, 0), "the inner view stops at its end");
    wheel(nested, 50, 50, -1);
    check(near(outer.scrollOffset().y, 40), "the wheel bubbles to the outer view at the inner limit");

    gui::UIContext wide;
    auto& sideways = wide.root().emplace<gui::ScrollView>(); sideways.setBounds({0, 0, 100, 100});
    sideways.setHorizontalScrolling(true); sideways.content().setLayout(gui::Layout::Horizontal);
    auto& strip = sideways.content().emplace<gui::Panel>(); strip.setPreferredSize({400, 50});
    wide.layout({100, 100, 100, 100});
    wheel(wide, 50, 50, 0, -1);
    check(near(sideways.maxScrollOffset().x, 300) && near(sideways.scrollOffset().x, 40) && near(strip.bounds().x, -40),
          "horizontal scrolling uses the horizontal wheel");
    check(near(sideways.viewport().height, 100 - gui::ScrollView::scrollbarWidth), "a horizontal bar reserves its own gutter");
}
void testTextField() {
    const auto font = gui::Font::load(MYGUI_TEST_FONT);
    gui::UIContext ui; ui.layout({400, 200, 400, 200});
    auto& field = ui.root().emplace<gui::TextField>(font); field.setBounds({10, 10, 200, 34});
    std::vector<std::string> changes; std::string submitted;
    field.setOnChange([&](const std::string& text) { changes.push_back(text); });
    field.setOnSubmit([&](const std::string& text) { submitted = text; });
    type(ui, U"ignored"); check(field.text().empty(), "unfocused fields ignore text input");
    click(ui, 50, 20); check(field.focused(), "clicking focuses the field");
    type(ui, U"caf\u00E9 \U0001F642");
    check(field.text() == "caf\xC3\xA9 \xF0\x9F\x99\x82" && field.caret() == field.text().size(), "typing inserts UTF-8 at the caret");
    check(changes.size() == 6 && changes.back() == field.text(), "each inserted character notifies");
    key(ui, gui::Key::Backspace, gui::KeyAction::Press); check(field.text() == "caf\xC3\xA9 ", "Backspace removes a whole supplementary code point");
    key(ui, gui::Key::Left, gui::KeyAction::Press); key(ui, gui::Key::Left, gui::KeyAction::Repeat);
    check(field.caret() == 3, "Left moves by code points, including repeats");
    key(ui, gui::Key::Delete, gui::KeyAction::Press); check(field.text() == "caf ", "Delete removes the two-byte code point after the caret");
    key(ui, gui::Key::Home, gui::KeyAction::Press); type(ui, U">");
    check(field.text() == ">caf " && field.caret() == 1, "Home moves to the start before inserting");
    key(ui, gui::Key::End, gui::KeyAction::Press); check(field.caret() == field.text().size(), "End moves to the end");
    const auto count = changes.size();
    key(ui, gui::Key::Backspace, gui::KeyAction::Release); key(ui, gui::Key::Home, gui::KeyAction::Press);
    key(ui, gui::Key::Backspace, gui::KeyAction::Press); key(ui, gui::Key::Left, gui::KeyAction::Press);
    check(changes.size() == count && field.caret() == 0, "navigation and no-op deletion do not notify");
    type(ui, U"\n\t\u0007\u007F"); check(field.text() == ">caf ", "control characters are not inserted");
    key(ui, gui::Key::Enter, gui::KeyAction::Press); key(ui, gui::Key::Enter, gui::KeyAction::Repeat);
    check(submitted == ">caf ", "Enter submits the text");
    check(press(ui, gui::Key::V), "unmodified keys are consumed so app shortcuts do not fire");
    check(!press(ui, gui::Key::Escape) && !press(ui, gui::Key::PageDown) && !press(ui, gui::Key::Q, {.control = true}),
          "Escape, paging and modifier shortcuts still bubble");

    field.setText("ab\ncd\xFF"); check(field.text() == "abcd\xEF\xBF\xBD" && field.caret() == field.text().size(),
                                          "setText cleans the line and moves the caret to the end");
    check(changes.size() == count, "setText never notifies");
    field.setCaret(5); check(field.caret() == 4, "setCaret snaps back to a code point boundary");
    field.setCaret(99); check(field.caret() == field.text().size(), "setCaret clamps to the text");
    field.setText("iiii WWWW");
    const auto offsets = font->caretOffsets("iiii WWWW", 16);
    click(ui, 10 + 8 + offsets[5] + 1, 20); check(field.caret() == 5, "clicks place the caret at the nearest boundary");
    click(ui, 300, 20); check(field.caret() == 5, "clicks outside the field leave the caret");
    click(ui, 15, 20); check(field.caret() == 0, "clicks before the text place the caret at the start");

    auto& narrow = ui.root().emplace<gui::TextField>(font); narrow.setBounds({10, 100, 60, 34});
    ui.requestFocus(&narrow); type(ui, U"a long line of text that overflows");
    click(ui, 10 + 8 + 44 - 1, 110);
    check(narrow.caret() == narrow.text().size(), "after scrolling, a click near the right edge maps through the scroll offset");
    narrow.setEnabled(false); type(ui, U"x"); check(narrow.text().back() == 's', "disabled fields ignore input");
}
}
int main() {
    try { testLayout(); testButtonsAndFocus(); testRoutingAndClips(); testTreeMutations(); testThemeAndCheckbox(); testSlider(); testScrollView(); testTextField(); }
    catch (const std::exception& error) { std::fprintf(stderr, "Error: %s\n", error.what()); return 1; }
    return failures ? 1 : 0;
}
