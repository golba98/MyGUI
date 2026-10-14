// Every built-in control inside a scroll view.
#include "Example.hpp"
#include "gui/Button.hpp"
#include "gui/Checkbox.hpp"
#include "gui/ImageView.hpp"
#include "gui/ProgressBar.hpp"
#include "gui/ScrollView.hpp"
#include "gui/Slider.hpp"
#include "gui/TextField.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    try {
        const int frames = example::frameLimit(argc, argv);
        const auto font = example::loadFont(argv[0]);
        gui::Window window{520, 640, "MyGUI widgets"};
        window.setSwapInterval(1);
        gui::Renderer renderer{window.glProcLoader()};

        gui::UIContext ui;
        ui.root().setLayout(gui::Layout::Vertical);
        ui.root().setBackgroundColor(ui.theme().background);
        auto& view = ui.root().emplace<gui::ScrollView>();
        view.setFlex(1);
        auto& page = view.content();
        page.setPadding({24, 24, 24, 24});
        page.setSpacing(14);

        const auto heading = [&](const char* text) { page.emplace<gui::Label>(text, font).setFontSize(20); };
        heading("Buttons and toggles");
        auto& status = page.emplace<gui::Label>("Nothing clicked yet", font);
        status.setTextColor(ui.theme().mutedText);
        page.emplace<gui::Button>("Button", font).setOnClick([&] { status.setText("Button clicked"); });
        page.emplace<gui::Checkbox>("Checkbox", font).setOnChange([&](bool on) { status.setText(on ? "Checked" : "Unchecked"); });
        page.emplace<gui::Switch>("Switch", font, true);

        heading("Values");
        auto& slider = page.emplace<gui::Slider>(0.0f, 1.0f, 0.3f);
        auto& progress = page.emplace<gui::ProgressBar>(0.3f);
        slider.setOnChange([&](float value) { progress.setValue(value); });

        heading("Text");
        auto& field = page.emplace<gui::TextField>(font);
        field.setPlaceholder("Type, select, copy and paste");
        field.setOnSubmit([&](const std::string& text) { status.setText("Submitted: " + text); });

        heading("Image");
        std::vector<std::uint8_t> pixels;
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x)
            pixels.insert(pixels.end(), {static_cast<std::uint8_t>(x * 2), static_cast<std::uint8_t>(y * 4), 200, 255});
        page.emplace<gui::ImageView>(gui::Image::fromPixels(128, 64, std::move(pixels)));

        heading("A long list");
        for (int i = 1; i <= 20; ++i) page.emplace<gui::Checkbox>("Item " + std::to_string(i), font);

        example::run(window, renderer, ui, frames);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
