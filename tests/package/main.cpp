// Uses only installed headers and needs no display.
#include <gui/Button.hpp>
#include <gui/ComboBox.hpp>
#include <gui/UIContext.hpp>

#include <cstdio>

int main() {
    gui::UIContext ui;
    ui.root().setLayout(gui::Layout::Vertical);
    auto& button = ui.root().emplace<gui::Button>("Installed");
    ui.layout({200, 100, 200, 100});
    bool completed = false;
    auto dialog = std::make_unique<gui::Dialog>("Installed API");
    dialog->addAction("save", "Save"); dialog->addCancelAction();
    const auto handle = ui.openDialog(std::move(dialog),
        [&](gui::Dialog&, const gui::DialogResult& result) { completed = !result.action; });
    ui.closeOverlay(handle);
    auto& combo=ui.root().emplace<gui::ComboBox>();
    combo.setItems({{"a","Installed ComboBox"}});
    const bool selected=combo.setSelectedId("a") && combo.selectedId()=="a";
    const bool ok = selected && button.bounds().width == 200 && completed && !ui.closeOverlay(handle);
    std::printf("%s\n", ok ? "MyGUI package OK" : "MyGUI package layout mismatch");
    return ok ? 0 : 1;
}
