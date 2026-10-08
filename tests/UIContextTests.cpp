// These tests never create a window or an OpenGL context.
#include "gui/Button.hpp"
#include "gui/UIContext.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>
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
void key(gui::UIContext& ui, gui::Key key, gui::KeyAction action, bool shift = false) {
    ui.handleEvent(gui::Event{gui::KeyEvent{key, 0, action, {.shift = shift}}});
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
}
int main() {
    try { testLayout(); testButtonsAndFocus(); testRoutingAndClips(); testTreeMutations(); }
    catch (const std::exception& error) { std::fprintf(stderr, "Error: %s\n", error.what()); return 1; }
    return failures ? 1 : 0;
}
