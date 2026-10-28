#include "Example.hpp"
#include "gui/ComboBox.hpp"
#include <iostream>
namespace {
class Page : public gui::Container {
public:
    gui::Container& main=emplace<gui::Container>();
    gui::ComboBox& edge=emplace<gui::ComboBox>();
    void arrange() override {
        const auto b=bounds(); main.setBounds({20,20,std::max(0.0f,b.width-40),std::max(0.0f,b.height-110)});
        edge.setBounds({std::max(0.0f,b.width-200),std::max(0.0f,b.height-52),180,32});
        Container::arrange();
    }
};
}
int main(int argc,char** argv) {try {
    const auto font=example::loadFont(argv[0]);
    gui::Window window{640,520,"MyGUI ComboBox"}; window.setSwapInterval(1);
    gui::Renderer renderer{window.glProcLoader()};
    auto page=std::make_unique<Page>(); auto* p=page.get(); gui::UIContext ui{std::move(page)};
    p->setBackgroundColor(ui.theme().background);
    auto& main=p->main; main.setLayout(gui::Layout::Vertical); main.setSpacing(12); main.setBackgroundColor({0,0,0,0});
    auto& status=main.emplace<gui::Label>("Choose an option (arrows, Enter, Escape, or type a prefix).",font);
    const std::vector<gui::ComboBoxItem> choices{{"red","Red"},{"off","Unavailable",false},{"blue","Blue"},{"other","Blue"}};
    auto& normal=main.emplace<gui::ComboBox>(font,choices); normal.setPlaceholder("Choose a color…");
    normal.setOnChange([&](const auto& item){status.setText("Selected: "+item.label+" ["+item.id+"]");});
    std::vector<gui::ComboBoxItem> many;
    for(int i=1;i<=60;++i) many.push_back({std::to_string(i),"Option "+std::to_string(i),i%7!=0});
    auto& longList=main.emplace<gui::ComboBox>(font,many); longList.setPlaceholder("Long scrolling list");
    p->edge.setFont(font); p->edge.setItems(many); p->edge.setPlaceholder("Bottom/right edge");
    main.emplace<gui::Button>("Open dialog",font).setOnClick([&]{
        auto dialog=std::make_unique<gui::Dialog>("Modal ComboBox",font);
        dialog->content().emplace<gui::ComboBox>(font,choices);
        dialog->addCancelAction("Close"); ui.openDialog(std::move(dialog));
    });
    int clicks=0;
    main.emplace<gui::Button>("Outside click counter",font).setOnClick([&]{status.setText("Outside clicks: "+std::to_string(++clicks));});
    example::run(window,renderer,ui,example::frameLimit(argc,argv));
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
