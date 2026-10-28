#include "gui/ComboBox.hpp"
#include "gui/UIContext.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
bool key(gui::UIContext& ui,gui::Key k,gui::Modifiers mods={}) {
    const bool handled=ui.handleEvent(gui::Event{gui::KeyEvent{k,0,gui::KeyAction::Press,mods}});
    ui.handleEvent(gui::Event{gui::KeyEvent{k,0,gui::KeyAction::Release,mods}}); return handled;
}
int main() {try {
    gui::UIContext ui; ui.layout({600,500,600,500});
    auto now=std::chrono::steady_clock::time_point{};
    auto& combo=ui.root().emplace<gui::ComboBox>(nullptr,std::vector<gui::ComboBoxItem>{{"a","Alpha"},{"x","Disabled",false},{"b","Beta"},{"c","Beta"},{"u","Éclair"}},[&]{return now;});
    combo.setBounds({20,20,180,32}); auto& next=ui.root().emplace<gui::Button>("next"); next.setBounds({220,20,100,32});
    ui.requestFocus(&combo); int changes=0; std::string committed;
    combo.setOnChange([&](const auto& item){++changes;committed=item.id;});
    check(key(ui,gui::Key::Enter) && combo.popupOpen() && !combo.selectedId(),"opening key cannot commit");
    check(combo.highlightedId()=="a","initial highlight"); key(ui,gui::Key::Down); check(combo.highlightedId()=="b" && !combo.selectedId(),"skip disabled and preview");
    key(ui,gui::Key::Enter); check(combo.selectedId()=="b" && changes==1,"commit once");
    key(ui,gui::Key::Space); key(ui,gui::Key::Enter); check(changes==1,"same selection no notification");
    combo.openPopup(); key(ui,gui::Key::End); key(ui,gui::Key::Down); check(combo.highlightedId()=="u","end clamped");
    key(ui,gui::Key::Escape); check(combo.selectedId()=="b","cancel preserves");
    combo.openPopup(); key(ui,gui::Key::Home); check(combo.highlightedId()=="a","home");
    ui.handleEvent(gui::Event{gui::TextInputEvent{U'B'}}); check(combo.highlightedId()=="b","case insensitive typeahead");
    now+=std::chrono::milliseconds(749); ui.handleEvent(gui::Event{gui::TextInputEvent{U'e'}}); check(combo.highlightedId()=="b","prefix retained before timeout");
    now+=std::chrono::milliseconds(750); ui.handleEvent(gui::Event{gui::TextInputEvent{U'É'}}); check(combo.highlightedId()=="u","timeout and Unicode");
    key(ui,gui::Key::Tab); check(next.focused() && !combo.popupOpen(),"Tab traverses once");
    key(ui,gui::Key::Tab,{.shift=true}); check(combo.focused(),"reverse Tab");
    combo.openPopup(); check(key(ui,gui::Key::Q,{.control=true}),"no shortcut leakage");
    check(!combo.setSelectedId("missing") && combo.popupOpen(),"invalid setter unchanged");
    combo.setSelectedId("a"); check(!combo.popupOpen() && changes==1,"programmatic selection closes silently");
    ui.markDrawn(); combo.openPopup(); ui.markDrawn();
    key(ui,gui::Key::Q,{.control=true}); check(!ui.needsRedraw(),"swallowed shortcut stays idle");
    key(ui,gui::Key::Escape);
    combo.setSelectedId(std::nullopt);
    auto mouse=[&](double x,double y,gui::ButtonAction action){return ui.handleEvent(gui::Event{gui::MouseButtonEvent{gui::MouseButton::Left,action,x,y}});};
    mouse(40,35,gui::ButtonAction::Press); mouse(40,35,gui::ButtonAction::Release);
    check(combo.popupOpen() && !combo.selectedId(),"pointer opening does not select");
    ui.handleEvent(gui::Event{gui::MouseMoveEvent{40,138}});
    check(combo.highlightedId()=="b" && combo.focused(),"row hover previews without focus transfer");
    mouse(40,138,gui::ButtonAction::Press); mouse(40,138,gui::ButtonAction::Release);
    check(combo.selectedId()=="b" && !combo.popupOpen(),"row click commits");
    combo.setSelectedId("a"); combo.openPopup(); bool duplicate=false;
    try {combo.setItems({{"same","one"},{"same","two"}});} catch(const std::invalid_argument&){duplicate=true;}
    check(duplicate && combo.popupOpen(),"duplicate IDs rejected atomically");
    combo.setItems({{"a","renamed"},{"b","Beta"}}); check(combo.selectedId()=="a" && !combo.popupOpen(),"replacement preserves ID");
    combo.setItems({{"a","disabled",false}}); check(!combo.selectedId() && !combo.openPopup(),"disabled only safe");
    combo.setItems({}); check(!combo.openPopup(),"empty safe");
    combo.setItems({{"a","Alpha"},{"b","Beta"}});
    combo.setOnChange([&](const auto& item){committed=item.id;combo.setItems({{"z","new"}});check(item.id=="b","callback payload retained");});
    combo.openPopup(); key(ui,gui::Key::Down); key(ui,gui::Key::Enter); check(committed=="b" && combo.items()[0].id=="z","callback replaces items");
    auto dialog=std::make_unique<gui::Dialog>("modal");
    auto& modalCombo=dialog->content().emplace<gui::ComboBox>(nullptr,std::vector<gui::ComboBoxItem>{{"a","Alpha"},{"b","Beta"}});
    auto dh=ui.openDialog(std::move(dialog));
    check(!combo.openPopup() && modalCombo.openPopup(),"modal ownership");
    key(ui,gui::Key::Escape); check(!modalCombo.popupOpen() && ui.isOverlayOpen(dh),"Escape closes popup first");
    modalCombo.openPopup(); auto newer=ui.openDialog(std::make_unique<gui::Dialog>("new"));
    check(!modalCombo.popupOpen(),"new modal cancels popup"); ui.closeOverlay(newer);
    modalCombo.openPopup(); ui.closeOverlay(dh); check(!ui.isOverlayOpen(dh),"owning dialog closes safely");
    auto& chaining=ui.root().emplace<gui::ComboBox>(nullptr,std::vector<gui::ComboBoxItem>{{"a","Alpha"}});
    chaining.setBounds({20,100,180,32});
    chaining.setOnChange([&](const auto&){combo.openPopup();});
    chaining.openPopup(); key(ui,gui::Key::Enter);
    check(combo.popupOpen() && !chaining.popupOpen(),"selection callback opens another popup"); combo.closePopup();
    {
        gui::UIContext listUI; listUI.layout({400,400,800,600});
        std::vector<gui::ComboBoxItem> items;
        for(int i=0;i<30;++i) items.push_back({std::to_string(i),"Item"+std::to_string(i)});
        auto& list=listUI.root().emplace<gui::ComboBox>(nullptr,items); list.setBounds({20,20,180,32});
        list.openPopup(); key(listUI,gui::Key::End);
        listUI.handleEvent(gui::Event{gui::MouseMoveEvent{40,275}});
        auto* row=dynamic_cast<gui::Label*>(listUI.hoveredWidget());
        check(row && row->text()=="Item29","keyboard reveals last row in scrolling popup");
        key(listUI,gui::Key::Enter); check(list.selectedId()=="29","long list commit");
    }
    auto& removed=ui.root().emplace<gui::ComboBox>(nullptr,std::vector<gui::ComboBoxItem>{{"a","Alpha"}}); removed.setBounds({20,100,180,32});
    removed.setOnChange([&](const auto&){ui.root().remove(removed);ui.openDialog(std::make_unique<gui::Dialog>("callback"));});
    removed.openPopup(); key(ui,gui::Key::Enter); check(!ui.dispatching(),"callback removes owner and opens dialog");
    std::puts("ComboBox tests passed");
} catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;} }
