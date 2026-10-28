#include "gui/Dialog.hpp"
#include "gui/Event.hpp"
#include "gui/Renderer.hpp"
#include "gui/Theme.hpp"
#include <algorithm>

namespace gui {
namespace {
class DialogTitle : public Label {
public:
    using Label::Label;
    void draw(Renderer& renderer) const override { if (visible()) drawText(renderer, theme().text); }
};
}
Dialog::Dialog(std::string title, std::shared_ptr<Font> font) : font_{std::move(font)} {
    setBackgroundColor({0,0,0,0});
    setPreferredSize({480,0});
    outer_ = &emplace<ScrollView>();
    outer_->setBackgroundColor({0,0,0,0});
    outer_->setHorizontalScrolling(true);
    sheet_ = &outer_->content();
    sheet_->setLayout(Layout::Absolute);
    title_ = &sheet_->emplace<DialogTitle>(std::move(title), font_);
    title_->setFontSize(20); title_->setWrap(true);
    body_ = &sheet_->emplace<ScrollView>();
    body_->setBackgroundColor({0,0,0,0});
    body_->setHorizontalScrolling(true);
    actions_ = &sheet_->emplace<Container>();
    actions_->setBackgroundColor({0,0,0,0});
}
Button& Dialog::addAction(std::string id, std::string label) {
    auto& button = actions_->emplace<Button>(std::move(label), font_);
    button.setOnClick([this, id = std::move(id)] { close({id}); });
    return button;
}
Button& Dialog::addCancelAction(std::string label) {
    auto& button = actions_->emplace<Button>(std::move(label), font_);
    button.setOnClick([this] { close(); });
    return button;
}
bool Dialog::close(DialogResult result) {
    const auto callback = close_;
    return callback && callback(std::move(result));
}
void Dialog::arrange() {
    const auto& t = theme();
    const float margin = t.dialogMargin, pad = t.dialogPadding, gap = t.dialogSpacing;
    const auto b = bounds();
    const float viewWidth = std::min(std::max(0.0f, b.width - 2*margin), preferredSize().width);
    float minimumActionWidth = 0;
    for (const auto& action : actions_->children()) if (action->visible())
        minimumActionWidth = std::max(minimumActionWidth, action->minimumSize().width);
    const float width = std::max(viewWidth, 2*pad + minimumActionWidth);
    const float inner = std::max(0.0f, width - 2*pad);
    actions_->setSpacing(gap);
    actions_->setLayout(Layout::Horizontal);
    if (actions_->preferredSize().width > inner) actions_->setLayout(Layout::Vertical);
    const float titleHeight = title_->heightForWidth(inner);
    const float actionHeight = actions_->heightForWidth(inner);
    const float chrome = 2*pad + titleHeight + actionHeight + 2*gap;
    const float available = std::max(0.0f, b.height - 2*margin);
    const float bodyHeight = std::min(content().heightForWidth(inner), std::max(80.0f, available - chrome));
    const float height = chrome + bodyHeight;
    const float viewHeight = std::min(available, height);
    outer_->setBounds({b.x+(b.width-viewWidth)/2, b.y+(b.height-viewHeight)/2, viewWidth, viewHeight});
    sheet_->setPreferredSize({width,height});
    outer_->arrange();
    const auto s = sheet_->bounds();
    title_->setBounds({s.x+pad,s.y+pad,inner,titleHeight});
    body_->setBounds({s.x+pad,s.y+pad+titleHeight+gap,inner,bodyHeight});
    actions_->setBounds({s.x+pad,s.y+pad+titleHeight+gap+bodyHeight+gap,inner,actionHeight});
    body_->arrange(); actions_->arrange();
}
void Dialog::draw(Renderer& renderer) const {
    if (!visible()) return;
    const auto& t = theme();
    renderer.drawRect(bounds(), t.dialogBackdrop);
    renderer.pushClip(outer_->bounds());
    try {
        renderer.drawRoundedRect(sheet_->bounds(), t.cornerRadius, t.surface);
        renderer.drawBorder(sheet_->bounds(), 1, t.border, t.cornerRadius);
        Container::draw(renderer);
    } catch (...) { renderer.popClip(); throw; }
    renderer.popClip();
}
bool Dialog::onEvent(const Event& event, UIContext&) {
    if (const auto* key = event.getIf<KeyEvent>(); key && escape_ && key->key == Key::Escape && key->action == KeyAction::Press)
        return close();
    if (const auto* mouse = event.getIf<MouseButtonEvent>(); mouse && backdrop_ && mouse->button == MouseButton::Left
        && mouse->action == ButtonAction::Press && !outer_->bounds().contains(mouse->x, mouse->y)) return close();
    return false;
}
} // namespace gui
