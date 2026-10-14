// Uses only installed headers and needs no display.
#include <gui/Button.hpp>
#include <gui/UIContext.hpp>

#include <cstdio>

int main() {
    gui::UIContext ui;
    ui.root().setLayout(gui::Layout::Vertical);
    auto& button = ui.root().emplace<gui::Button>("Installed");
    ui.layout({200, 100, 200, 100});
    const bool ok = button.bounds().width == 200;
    std::printf("%s\n", ok ? "MyGUI package OK" : "MyGUI package layout mismatch");
    return ok ? 0 : 1;
}
