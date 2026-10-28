// Modal composition, editable content, and nested confirmation.
#include "Example.hpp"
#include "gui/Dialog.hpp"
#include "gui/TextField.hpp"
#include "gui/Slider.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto font = example::loadFont(argv[0]);
        gui::Window window{640, 480, "MyGUI dialogs"};
        window.setSwapInterval(1);
        gui::Renderer renderer{window.glProcLoader()};
        gui::UIContext ui;
        auto& root = ui.root(); root.setLayout(gui::Layout::Vertical);
        root.setPadding({24,24,24,24}); root.setSpacing(12);
        root.setBackgroundColor(ui.theme().background);
        auto& status = root.emplace<gui::Label>("Open a dialog; background controls pause.", font);
        int count = 0;
        root.emplace<gui::Button>("Background counter", font).setOnClick([&] {
            status.setText("Background clicks: " + std::to_string(++count));
        });
        root.emplace<gui::Slider>();
        root.emplace<gui::Button>("Confirmation", font).setOnClick([&] {
            auto dialog = std::make_unique<gui::Dialog>("Confirm", font);
            dialog->content().emplace<gui::Label>("Continue with this operation?", font);
            dialog->addAction("confirm", "Confirm"); dialog->addCancelAction();
            ui.openDialog(std::move(dialog), [&](gui::Dialog&, const gui::DialogResult& result) {
                status.setText(result.action ? "Confirmed" : "Cancelled");
            });
        });
        root.emplace<gui::Button>("Settings", font).setOnClick([&] {
            auto dialog = std::make_unique<gui::Dialog>("Settings", font);
            dialog->content().setSpacing(12);
            dialog->content().emplace<gui::Label>("Name (selection and clipboard work here)", font);
            auto& name = dialog->content().emplace<gui::TextField>(font, "MyGUI");
            dialog->addAction("save", "Save"); dialog->addCancelAction();
            dialog->content().emplace<gui::Button>("Open nested confirmation", font).setOnClick([&] {
                auto nested = std::make_unique<gui::Dialog>("Nested confirmation", font);
                nested->content().emplace<gui::Label>("Closing returns to Settings.", font);
                nested->addAction("confirm", "Confirm"); nested->addCancelAction();
                ui.openDialog(std::move(nested));
            });
            ui.openDialog(std::move(dialog), [&, field = &name](gui::Dialog&, const gui::DialogResult& result) {
                status.setText(result.action == "save" ? "Saved: " + field->text() : "Settings cancelled");
            });
        });
        example::run(window, renderer, ui, example::frameLimit(argc, argv));
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
