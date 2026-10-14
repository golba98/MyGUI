// The smallest MyGUI program: a label and a button that changes it.
#include "Example.hpp"
#include "gui/Button.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const int frames = example::frameLimit(argc, argv);
        const auto font = example::loadFont(argv[0]);
        gui::Window window{480, 220, "MyGUI hello"};
        window.setSwapInterval(1);
        // Declared after the window so GPU resources are released first.
        gui::Renderer renderer{window.glProcLoader()};

        gui::UIContext ui;
        auto& root = ui.root();
        root.setLayout(gui::Layout::Vertical);
        root.setPadding({24, 24, 24, 24});
        root.setSpacing(16);
        root.setBackgroundColor(ui.theme().background);

        auto& label = root.emplace<gui::Label>("Hello, MyGUI!", font);
        label.setFontSize(28);
        auto& row = root.emplace<gui::Container>();
        row.setLayout(gui::Layout::Horizontal);
        row.setBackgroundColor({0, 0, 0, 0});
        auto& button = row.emplace<gui::Button>("Click me", font);

        int clicks = 0;
        button.setOnClick([&] {
            ++clicks;
            label.setText("Clicked " + std::to_string(clicks) + (clicks == 1 ? " time" : " times"));
        });

        example::run(window, renderer, ui, frames);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
