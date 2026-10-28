#pragma once
#include "gui/Button.hpp"
#include "gui/ScrollView.hpp"
#include <optional>

namespace gui {
struct DialogResult { std::optional<std::string> action; };
// A viewport-sized modal host; content() and actions() own user widgets.
class Dialog : public Container {
public:
    explicit Dialog(std::string title, std::shared_ptr<Font> font = nullptr);
    void setTitle(std::string title) { title_->setText(std::move(title)); }
    Container& content() noexcept { return body_->content(); }
    Container& actions() noexcept { return *actions_; }
    Button& addAction(std::string id, std::string label);
    Button& addCancelAction(std::string label = "Cancel");
    void setDismissOnEscape(bool value) noexcept { escape_ = value; }
    void setDismissOnBackdropClick(bool value) noexcept { backdrop_ = value; }
    bool close(DialogResult result = {});
    Rect sheetBounds() const noexcept { return sheet_->bounds(); }
    void arrange() override;
    void draw(Renderer&) const override;
    bool onEvent(const Event&, UIContext&) override;
private:
    friend class UIContext;
    std::function<bool(DialogResult)> close_;
    std::shared_ptr<Font> font_;
    ScrollView* outer_{};
    Container* sheet_{};
    Label* title_{};
    ScrollView* body_{};
    Container* actions_{};
    bool escape_{true}, backdrop_{false};
};
} // namespace gui
