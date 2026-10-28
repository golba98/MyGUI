// Event-driven regression tests: no window or GL initialization.
#include "gui/UIContext.hpp"
#include "gui/TextField.hpp"
#include "gui/Slider.hpp"
#include <cstdio>
#include <stdexcept>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool key(gui::UIContext& ui, gui::Key k, gui::KeyAction action = gui::KeyAction::Press, gui::Modifiers mods = {}) {
    return ui.handleEvent(gui::Event{gui::KeyEvent{k,0,action,mods}});
}
bool mouse(gui::UIContext& ui, double x, double y, gui::ButtonAction action) {
    return ui.handleEvent(gui::Event{gui::MouseButtonEvent{gui::MouseButton::Left,action,x,y}});
}
void click(gui::UIContext& ui, const gui::Rect& b) {
    mouse(ui,b.x+b.width/2,b.y+b.height/2,gui::ButtonAction::Press);
    mouse(ui,b.x+b.width/2,b.y+b.height/2,gui::ButtonAction::Release);
}
struct Clipboard : gui::Clipboard {
    std::string value;
    std::string text() const override { return value; }
    void setText(std::string_view text) override { value = text; }
};
void routingAndFocus() {
    gui::UIContext ui; ui.layout({640,480,640,480});
    auto& background = ui.root().emplace<gui::Button>("background"); background.setBounds({0,0,640,480});
    int clicks=0; background.setOnClick([&] { ++clicks; });
    mouse(ui,5,5,gui::ButtonAction::Press);
    check(background.pressed(),"background press");
    auto dialog = std::make_unique<gui::Dialog>("Input");
    auto& field = dialog->content().emplace<gui::TextField>();
    auto& hidden = dialog->content().emplace<gui::Button>("hidden"); hidden.setVisible(false);
    auto& disabled = dialog->content().emplace<gui::Button>("disabled"); disabled.setEnabled(false);
    auto& save = dialog->addAction("save","Save"); dialog->addCancelAction();
    int completions=0;
    auto handle = ui.openDialog(std::move(dialog),[&](gui::Dialog&,const gui::DialogResult& result) {
        ++completions; check(result.action == "save","action result"); check(field.text()=="hello","field retained in completion");
    });
    check(!background.pressed() && !background.hovered() && !ui.capturedWidget(),"opening cancels background press");
    check(field.focused() && !ui.requestFocus(&background) && !ui.capturePointer(background),"modal focus boundary");
    mouse(ui,5,5,gui::ButtonAction::Release);
    check(clicks==0,"release quarantined");
    check(key(ui,gui::Key::Q,gui::KeyAction::Press,{.control=true}),"app shortcuts blocked");
    check(ui.handleEvent(gui::Event{gui::MouseScrollEvent{0,-1,5,5}}),"background wheel blocked");
    mouse(ui,5,5,gui::ButtonAction::Press); mouse(ui,5,5,gui::ButtonAction::Release);
    check(ui.isOverlayOpen(handle) && clicks==0,"backdrop default blocks without dismissal");
    ui.requestFocus(&field);
    Clipboard clipboard; ui.setClipboard(&clipboard);
    for (auto c : U"hello") if(c) ui.handleEvent(gui::Event{gui::TextInputEvent{c}});
    key(ui,gui::Key::A,gui::KeyAction::Press,{.control=true});
    key(ui,gui::Key::C,gui::KeyAction::Press,{.control=true});
    check(clipboard.value=="hello" && field.hasSelection(),"modal selection and copy");
    key(ui,gui::Key::X,gui::KeyAction::Press,{.control=true});
    key(ui,gui::Key::V,gui::KeyAction::Press,{.control=true});
    check(field.text()=="hello","modal cut and paste");
    int submitted=0; field.setOnSubmit([&](const std::string&){++submitted;});
    key(ui,gui::Key::Enter); key(ui,gui::Key::Enter,gui::KeyAction::Release);
    check(submitted==1 && ui.isOverlayOpen(handle),"Enter respects text field");
    key(ui,gui::Key::Tab); check(save.focused(),"Tab skips unavailable widgets");
    key(ui,gui::Key::Tab,gui::KeyAction::Release);
    key(ui,gui::Key::Tab,gui::KeyAction::Press,{.shift=true}); check(field.focused(),"reverse traversal");
    click(ui,save.bounds());
    check(completions==1 && background.focused() && !ui.isOverlayOpen(handle),"completion and restored focus");
    check(!ui.closeOverlay(handle) && completions==1,"repeated close is harmless");
    gui::UIContext other; check(!other.closeOverlay(handle),"foreign handle rejected");
}
void lifecycle() {
    gui::UIContext ui; ui.layout({640,480,640,480});
    auto& opener=ui.root().emplace<gui::Button>("opener"); ui.requestFocus(&opener);
    auto lower=std::make_unique<gui::Dialog>("lower");
    auto& first=lower->addAction("ok","OK");
    auto low=ui.openDialog(std::move(lower));
    auto upper=std::make_unique<gui::Dialog>("upper"); upper->addCancelAction();
    auto high=ui.openDialog(std::move(upper));
    ui.closeOverlay(high); check(first.focused(),"stack restores lower focus");
    ui.root().remove(opener);
    ui.closeOverlay(low); check(!ui.focusedWidget(),"removed saved target cleared");
    gui::OverlayHandle next;
    auto chain=std::make_unique<gui::Dialog>("chain");
    auto h=ui.openDialog(std::move(chain),[&](gui::Dialog&, const gui::DialogResult&){
        next=ui.openDialog(std::make_unique<gui::Dialog>("next"));
    });
    ui.closeOverlay(h); check(ui.isOverlayOpen(next),"completion may open another dialog");
    check(key(ui,gui::Key::Tab),"empty modal traps Tab");
    key(ui,gui::Key::Tab,gui::KeyAction::Release);
    check(key(ui,gui::Key::Escape) && !ui.isOverlayOpen(next),"Escape closes childless dialog");
    check(key(ui,gui::Key::Escape,gui::KeyAction::Release),"Escape release does not fall through");
    auto policy=std::make_unique<gui::Dialog>("policy");
    policy->setDismissOnEscape(false); policy->setDismissOnBackdropClick(true);
    auto p=ui.openDialog(std::move(policy));
    key(ui,gui::Key::Escape); check(ui.isOverlayOpen(p),"Escape disabled");
    key(ui,gui::Key::Escape,gui::KeyAction::Release);
    mouse(ui,0,0,gui::ButtonAction::Press); check(!ui.isOverlayOpen(p),"backdrop dismisses on press");
    check(mouse(ui,0,0,gui::ButtonAction::Release),"backdrop release consumed");
    auto capture=std::make_unique<gui::Dialog>("capture");
    auto& slider=capture->content().emplace<gui::Slider>();
    auto cap=ui.openDialog(std::move(capture));
    ui.layout({640,480,640,480});
    mouse(ui,slider.bounds().x+10,slider.bounds().y+10,gui::ButtonAction::Press);
    check(ui.capturedWidget()==&slider,"child captures");
    ui.closeOverlay(cap); check(!ui.capturedWidget(),"close clears child capture");
    mouse(ui,0,0,gui::ButtonAction::Release);
    int calls=0;
    { gui::UIContext temporary; temporary.openDialog(std::make_unique<gui::Dialog>("teardown"),
        [&](gui::Dialog&,const gui::DialogResult&){++calls;}); }
    check(calls==0,"teardown never invokes completion");
    auto throwing=ui.openDialog(std::make_unique<gui::Dialog>("throw"),
        [](gui::Dialog&,const gui::DialogResult&){throw std::runtime_error("expected");});
    bool caught=false; try { ui.closeOverlay(throwing); } catch (...) { caught=true; }
    check(caught && !ui.dispatching() && !ui.closeOverlay(throwing),"throwing completion cleans up once");
}
void availabilityAndOverlays() {
    gui::UIContext ui; ui.layout({640,480,640,480});
    auto& background=ui.root().emplace<gui::Button>("background"); background.setBounds({0,0,640,480});
    int backgroundClicks=0, overlayClicks=0;
    background.setOnClick([&]{++backgroundClicks;});
    auto tree=std::make_unique<gui::Container>(); tree->setBackgroundColor({0,0,0,0});
    auto& overlay=tree->emplace<gui::Button>("overlay"); overlay.setBounds({0,0,200,100});
    overlay.setOnClick([&]{++overlayClicks;}); auto oh=ui.openOverlay(std::move(tree));
    click(ui,overlay.bounds()); check(overlayClicks==1 && backgroundClicks==0,"overlay wins overlapping hit test");
    ui.closeOverlay(oh); ui.requestFocus(&background);
    auto lower=std::make_unique<gui::Dialog>("lower"); auto* lp=lower.get(); lower->addCancelAction();
    auto low=ui.openDialog(std::move(lower));
    lp->setVisible(false); ui.layout({640,480,640,480});
    check(background.focused(),"hidden modal restores background scope");
    lp->setVisible(true); ui.layout({640,480,640,480});
    check(!background.focused(),"shown modal reenters scope");
    lp->setEnabled(false); check(key(ui,gui::Key::Space),"disabled modal still blocks keys");
    lp->setEnabled(true);
    auto top=ui.openDialog(std::make_unique<gui::Dialog>("top"));
    ui.closeOverlay(low); check(ui.isOverlayOpen(top),"out-of-order close leaves top active");
    ui.closeOverlay(top);
    auto consume=std::make_unique<gui::Dialog>("consume");
    struct EscapeConsumer : gui::Button {
        bool onEvent(const gui::Event& e,gui::UIContext& context) override {
            if (const auto* k=e.getIf<gui::KeyEvent>(); k && k->key==gui::Key::Escape) return true;
            return Button::onEvent(e,context);
        }
    };
    consume->content().emplace<EscapeConsumer>(); auto c=ui.openDialog(std::move(consume));
    key(ui,gui::Key::Escape); check(ui.isOverlayOpen(c),"focused widget may consume Escape");
    key(ui,gui::Key::Escape,gui::KeyAction::Release); ui.closeOverlay(c);
    gui::OverlayHandle stale;
    { gui::UIContext temporary; stale=temporary.openDialog(std::make_unique<gui::Dialog>("old context")); }
    check(!ui.closeOverlay(stale),"handle remains harmless after owner destruction");
}
void blockedInteractions() {
    gui::UIContext ui; ui.layout({640,480,640,480});
    auto& slider=ui.root().emplace<gui::Slider>(); slider.setBounds({0,0,200,30});
    mouse(ui,20,15,gui::ButtonAction::Press); check(slider.dragging(),"background drag starts");
    auto h=ui.openDialog(std::make_unique<gui::Dialog>("drag boundary"));
    const float value=slider.value();
    ui.handleEvent(gui::Event{gui::MouseMoveEvent{190,15}});
    mouse(ui,190,15,gui::ButtonAction::Release);
    check(!slider.dragging() && slider.value()==value,"modal cancels drag and blocks motion");
    ui.closeOverlay(h);
    auto& view=ui.root().emplace<gui::ScrollView>(); view.setBounds({0,50,200,200});
    view.content().emplace<gui::Panel>().setPreferredSize({0,1000});
    auto& button=ui.root().emplace<gui::Button>("keyboard"); button.setBounds({300,0,150,40});
    int clicks=0; button.setOnClick([&]{++clicks;}); ui.requestFocus(&button);
    key(ui,gui::Key::Space); check(button.pressed(),"background keyboard press starts");
    auto modal=std::make_unique<gui::Dialog>("blocking"); modal->addCancelAction();
    h=ui.openDialog(std::move(modal));
    check(!button.pressed(),"modal cancels keyboard press");
    check(ui.handleEvent(gui::Event{gui::MouseScrollEvent{0,-3,10,100}}) && view.scrollOffset().y==0,"background scroll offset unchanged");
    key(ui,gui::Key::Space,gui::KeyAction::Release); check(clicks==0,"keyboard release cannot activate background");
    auto top=ui.openDialog(std::make_unique<gui::Dialog>("top"));
    key(ui,gui::Key::Escape);
    check(!ui.isOverlayOpen(top) && ui.isOverlayOpen(h),"Escape dismisses only top modal");
    key(ui,gui::Key::Escape,gui::KeyAction::Repeat);
    key(ui,gui::Key::Escape,gui::KeyAction::Release);
    check(ui.isOverlayOpen(h),"Escape repeat and release cannot dismiss lower modal");
    check(!ui.handleEvent(gui::Event{gui::WindowResizeEvent{800,600,800,600}}),"resize remains exposed");
    check(ui.isOverlayOpen(h),"resize preserves modal");
    ui.handleEvent(gui::Event{gui::WindowFocusEvent{false}});
    check(!ui.focusedWidget() && !ui.capturedWidget() && !ui.hoveredWidget(),"focus loss clears modal interactions");
    ui.handleEvent(gui::Event{gui::WindowFocusEvent{true}});
    check(ui.focusedWidget()!=nullptr,"focus regain enters modal");
    button.setEnabled(false); ui.closeOverlay(h);
    check(ui.focusedWidget()!=&button,"disabled saved focus not restored");
}
void redrawAndSize() {
    gui::UIContext ui; ui.layout({640,480,640,480}); ui.markDrawn();
    auto dialog=std::make_unique<gui::Dialog>("large"); auto* d=dialog.get();
    auto& panel=dialog->content().emplace<gui::Panel>(); panel.setPreferredSize({900,2000});
    auto& cancel=dialog->addCancelAction(); auto h=ui.openDialog(std::move(dialog));
    check(ui.needsRedraw(),"open redraw"); ui.markDrawn();
    ui.layout({640,480,640,480}); check(!ui.needsRedraw(),"settled layout idle");
    check(key(ui,gui::Key::Q,gui::KeyAction::Press,{.control=true}),"swallowed key");
    check(!ui.needsRedraw(),"swallowing key alone stays idle");
    ui.layout({400,300,800,600}); check(ui.needsRedraw(),"resize redraw");
    check(d->sheetBounds().width<=400,"dialog width constrained");
    ui.markDrawn(); panel.setBackgroundColor({1,0,0,1}); check(ui.needsRedraw(),"overlay setter redraw");
    ui.layout({100,120,100,120}); ui.reveal(cancel);
    const auto visible=gui::intersect(cancel.bounds(),{0,0,100,120});
    check(!visible.empty(),"actions revealed in tiny viewport");
    ui.markDrawn(); click(ui,visible);
    check(!ui.isOverlayOpen(h) && ui.needsRedraw(),"tiny viewport action remains clickable and close redraws");
}
}
int main() {
    try { routingAndFocus(); lifecycle(); availabilityAndOverlays(); blockedInteractions(); redrawAndSize(); }
    catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
    std::puts("Dialog tests passed");
}
