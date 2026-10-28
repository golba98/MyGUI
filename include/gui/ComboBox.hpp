#pragma once
#include "gui/Button.hpp"
#include "gui/Overlay.hpp"
#include <chrono>
#include <vector>
namespace gui {
struct ComboBoxItem { std::string id, label; bool enabled{true}; };
class ComboBox : public Button {
public:
    using TimeSource=std::function<std::chrono::steady_clock::time_point()>;
    explicit ComboBox(std::shared_ptr<Font> font=nullptr, std::vector<ComboBoxItem> items={}, TimeSource clock={});
    ComboBox(const ComboBox&);
    ComboBox& operator=(const ComboBox&);
    void setItems(std::vector<ComboBoxItem> items);
    const std::vector<ComboBoxItem>& items() const noexcept { return items_; }
    bool setSelectedId(std::optional<std::string> id);
    std::optional<std::string> selectedId() const { return selected_; }
    void setPlaceholder(std::string text);
    const std::string& placeholder() const noexcept { return placeholder_; }
    void setOnChange(std::function<void(const ComboBoxItem&)> callback) { onChange_=std::move(callback); }
    bool openPopup();
    bool closePopup();
    bool popupOpen() const noexcept;
    std::optional<std::string> highlightedId() const;
    bool onEvent(const Event&,UIContext&) override;
    void draw(Renderer&) const override;
private:
    class Row;
    using Button::setOnClick;
    using Label::setText;
    void activate() override { openPopup(); }
    void caption();
    void highlight(std::size_t);
    void commit(std::size_t);
    void search(char32_t);
    std::vector<ComboBoxItem> items_;
    std::optional<std::string> selected_;
    std::optional<std::size_t> highlighted_;
    std::string placeholder_{"Select…"};
    OverlayHandle popup_;
    std::vector<Row*> rows_;
    std::function<void(const ComboBoxItem&)> onChange_;
    TimeSource clock_;
    std::u32string prefix_;
    std::chrono::steady_clock::time_point lastInput_{};
};
}
