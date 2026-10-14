// Nested horizontal and vertical layouts with padding, spacing, flex weights
// and wrapped text. Resize the window to watch everything reflow.
#include "Example.hpp"
#include "gui/Button.hpp"

#include <exception>
#include <iostream>

namespace {
gui::Container& box(gui::Container& parent, gui::Layout layout, gui::Color color, float flex = 0) {
    auto& container = parent.emplace<gui::Container>();
    container.setLayout(layout);
    container.setPadding({12, 12, 12, 12});
    container.setSpacing(10);
    container.setBackgroundColor(color);
    container.setFlex(flex);
    return container;
}
} // namespace

int main(int argc, char** argv) {
    try {
        const int frames = example::frameLimit(argc, argv);
        const auto font = example::loadFont(argv[0]);
        gui::Window window{900, 560, "MyGUI layout"};
        window.setSwapInterval(1);
        gui::Renderer renderer{window.glProcLoader()};

        gui::UIContext ui;
        auto& root = ui.root();
        root.setLayout(gui::Layout::Vertical);
        root.setPadding({16, 16, 16, 16});
        root.setSpacing(12);
        root.setBackgroundColor(ui.theme().background);

        auto& header = box(root, gui::Layout::Horizontal, ui.theme().surface);
        auto& title = header.emplace<gui::Label>("Layout example", font);
        title.setFontSize(22);
        title.setFlex(1);
        header.emplace<gui::Button>("Header action", font);

        auto& body = box(root, gui::Layout::Horizontal, {0, 0, 0, 0}, 1);
        body.setPadding({});
        auto& sidebar = box(body, gui::Layout::Vertical, ui.theme().surface);
        sidebar.setPreferredSize({180, 0});
        sidebar.setMinimumSize({120, 0});
        for (const char* name : {"Overview", "Details", "Settings"}) sidebar.emplace<gui::Button>(name, font);

        auto& main = box(body, gui::Layout::Vertical, ui.theme().surface, 1);
        auto& text = main.emplace<gui::Label>(
            "The body is a horizontal row: a fixed-width sidebar and this flexible column. "
            "Below, three panels share the remaining width with flex weights 1, 2 and 1. "
            "Text in this column wraps to its width and pushes the panels down.", font);
        text.setWrap(true);
        auto& panels = box(main, gui::Layout::Horizontal, {0, 0, 0, 0}, 1);
        panels.setPadding({});
        const gui::Color colors[]{{0.25f, 0.45f, 0.70f, 1}, {0.30f, 0.60f, 0.45f, 1}, {0.70f, 0.40f, 0.30f, 1}};
        const float weights[]{1, 2, 1};
        for (int i = 0; i < 3; ++i) {
            auto& panel = panels.emplace<gui::Panel>();
            panel.setBackgroundColor(colors[i]);
            panel.setMinimumSize({40, 60});
            panel.setFlex(weights[i]);
        }

        auto& footer = root.emplace<gui::Label>("Hidden widgets reserve no space; minimum sizes overflow and clip.", font);
        footer.setFontSize(13);
        footer.setTextColor(ui.theme().mutedText);

        example::run(window, renderer, ui, frames);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
