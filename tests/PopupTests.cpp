#include "gui/UIContext.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
int main() { try {
    gui::UIContext ui; ui.layout({400,300,800,450});
    auto& owner=ui.root().emplace<gui::Button>("owner"); owner.setBounds({350,20,50,32});
    auto& outside=ui.root().emplace<gui::Button>("outside"); outside.setBounds({0,0,80,40});
    int outsideClicks=0; outside.setOnClick([&]{++outsideClicks;});
    auto popup=std::make_unique<gui::Popup>(); auto* p=popup.get();
    popup->content().emplace<gui::Panel>().setPreferredSize({200,500});
    auto h=ui.openPopup(owner,std::move(popup)); ui.layout({400,300,800,450});
    check(ui.isOverlayOpen(h) && p->bounds().y==54 && p->bounds().x==192,"below and horizontal clamp");
    check(p->bounds().height<=240 && p->maxScrollOffset().y>0,"height and scrolling");
    owner.setBounds({350,260,50,32}); ui.layout({400,300,800,450});
    check(p->bounds().y<owner.bounds().y,"placement above");
    ui.markDrawn(); ui.layout({400,300,800,450}); check(!ui.needsRedraw(),"idle popup");
    check(ui.handleEvent(gui::Event{gui::MouseButtonEvent{gui::MouseButton::Left,gui::ButtonAction::Press,0,0}}),"outside press consumed");
    check(!ui.isOverlayOpen(h) && ui.handleEvent(gui::Event{gui::MouseButtonEvent{gui::MouseButton::Left,gui::ButtonAction::Release,0,0}}),"outside release consumed");
    check(outsideClicks==0,"dismissal cannot click through");
    ui.handleEvent(gui::Event{gui::MouseButtonEvent{gui::MouseButton::Left,gui::ButtonAction::Press,10,10}});
    ui.handleEvent(gui::Event{gui::MouseButtonEvent{gui::MouseButton::Left,gui::ButtonAction::Release,10,10}});
    check(outsideClicks==1,"next fresh click works");
    auto& scroll=ui.root().emplace<gui::ScrollView>(); scroll.setBounds({0,0,200,150});
    auto& anchor=scroll.content().emplace<gui::Button>("anchor");
    scroll.content().emplace<gui::Panel>().setPreferredSize({0,1000});
    ui.layout({400,300,400,300});
    auto make=[] { auto result=std::make_unique<gui::Popup>(); result->content().emplace<gui::Panel>().setPreferredSize({100,100}); return result; };
    auto first=make(); p=first.get(); h=ui.openPopup(anchor,std::move(first));
    auto before=p->bounds().y;
    scroll.setScrollOffset({0,10}); ui.layout({400,300,400,300}); check(p->bounds().y!=before,"ancestor scroll repositions");
    scroll.setScrollOffset({0,200}); ui.layout({400,300,400,300}); check(!ui.isOverlayOpen(h),"clipped owner closes");
    owner.setBounds({20,20,100,32}); h=ui.openPopup(owner,make());
    auto next=ui.openPopup(owner,make()); check(!ui.isOverlayOpen(h) && ui.isOverlayOpen(next),"replacement");
    owner.setEnabled(false); check(!ui.isOverlayOpen(next),"disable closes"); owner.setEnabled(true);
    h=ui.openPopup(owner,make()); ui.root().remove(owner); check(!ui.isOverlayOpen(h),"remove closes");
    check(!ui.closeOverlay(h),"repeated close");
    auto& other=ui.root().emplace<gui::Button>("focus"); other.setBounds({20,20,100,32});
    h=ui.openPopup(other,make()); ui.handleEvent(gui::Event{gui::WindowFocusEvent{false}}); check(!ui.isOverlayOpen(h),"focus loss closes");
    std::puts("Popup tests passed");
} catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;} }
