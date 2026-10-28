#include "gui/ComboBox.hpp"
#include "gui/UIContext.hpp"
#include "gui/Renderer.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
namespace gui {
class ComboBox::Row : public Button {
public:
    Row(ComboBox& owner,std::size_t index) : Button(owner.items_[index].label,owner.font()),owner_{owner},index_{index} {
        setFontSize(owner.fontSize()); setFocusable(false); setEnabled(owner.items_[index].enabled);
        setAlignment(HorizontalAlign::Left,VerticalAlign::Center);
    }
    bool marked{};
    void draw(Renderer& renderer) const override {
        if (!visible()) return;
        if (marked) renderer.drawRoundedRect(bounds(),theme().cornerRadius,theme().accentHovered);
        const auto b=bounds(); drawText(renderer,enabled() ? theme().text : theme().disabledText,{b.x+8,b.y,std::max(0.0f,b.width-16),b.height});
    }
    bool onEvent(const Event& event,UIContext& context) override {
        if (enabled() && event.getIf<MouseMoveEvent>()) owner_.highlight(index_);
        return Button::onEvent(event,context);
    }
private:
    void activate() override { owner_.commit(index_); }
    ComboBox& owner_;
    std::size_t index_;
};
ComboBox::ComboBox(std::shared_ptr<Font> font,std::vector<ComboBoxItem> items,TimeSource clock)
    : Button("",std::move(font)),clock_{clock ? std::move(clock) : TimeSource{std::chrono::steady_clock::now}} {
    setAlignment(HorizontalAlign::Left,VerticalAlign::Center); setMinimumSize({120,32}); setItems(std::move(items));
}
ComboBox::ComboBox(const ComboBox& other) : Button(other),items_{other.items_},selected_{other.selected_},placeholder_{other.placeholder_},onChange_{other.onChange_},clock_{other.clock_} {}
ComboBox& ComboBox::operator=(const ComboBox& other) {
    if (this!=&other) {
        auto copy=other;
        closePopup(); Button::operator=(copy);
        items_=std::move(copy.items_); selected_=std::move(copy.selected_); placeholder_=std::move(copy.placeholder_);
        onChange_=std::move(copy.onChange_); clock_=std::move(copy.clock_);
        rows_.clear(); highlighted_.reset(); prefix_.clear(); popup_={};
    }
    return *this;
}
bool ComboBox::popupOpen() const noexcept { return attachedContext() && attachedContext()->isOverlayOpen(popup_); }
bool ComboBox::closePopup() { return attachedContext() && attachedContext()->closeOverlay(popup_); }
void ComboBox::caption() {
    const auto it=std::find_if(items_.begin(),items_.end(),[&](const auto& item){return selected_ && item.id==*selected_;});
    Label::setText(it==items_.end() ? placeholder_ : it->label);
}
void ComboBox::setPlaceholder(std::string text) { placeholder_=sanitizeUtf8(text); caption(); }
void ComboBox::setItems(std::vector<ComboBoxItem> items) {
    std::unordered_set<std::string> ids;
    for (auto& item:items) {
        if (!ids.insert(item.id).second) throw std::invalid_argument("ComboBox identifiers must be unique");
        item.label=sanitizeUtf8(item.label);
    }
    closePopup(); items_=std::move(items); rows_.clear(); highlighted_.reset(); prefix_.clear();
    if (selected_ && std::none_of(items_.begin(),items_.end(),[&](const auto& i){return i.enabled && i.id==*selected_;})) selected_.reset();
    caption();
}
bool ComboBox::setSelectedId(std::optional<std::string> id) {
    if (id && std::none_of(items_.begin(),items_.end(),[&](const auto& i){return i.enabled && i.id==*id;})) return false;
    closePopup(); selected_=std::move(id); caption(); return true;
}
std::optional<std::string> ComboBox::highlightedId() const {
    return popupOpen() && highlighted_ ? std::optional<std::string>{items_[*highlighted_].id} : std::nullopt;
}
bool ComboBox::openPopup() {
    if (popupOpen()) return true;
    auto* context=attachedContext();
    if (!context || std::none_of(items_.begin(),items_.end(),[](const auto& i){return i.enabled;})) return false;
    auto popup=std::make_unique<Popup>();
    rows_.clear(); prefix_.clear(); highlighted_.reset();
    for (std::size_t i=0;i<items_.size();++i) rows_.push_back(&popup->content().emplace<Row>(*this,i));
    popup_=context->openPopup(*this,std::move(popup));
    if (!popupOpen()) { rows_.clear(); return false; }
    context->requestFocus(this);
    auto it=std::find_if(items_.begin(),items_.end(),[&](const auto& i){return i.enabled && (!selected_ || i.id==*selected_);});
    highlight(static_cast<std::size_t>(it-items_.begin()));
    return true;
}
void ComboBox::highlight(std::size_t index) {
    if (!popupOpen() || index>=items_.size() || !items_[index].enabled || highlighted_==index) return;
    if (highlighted_) rows_[*highlighted_]->marked=false;
    highlighted_=index; rows_[index]->marked=true;
    attachedContext()->requestRedraw(); attachedContext()->reveal(*rows_[index]);
}
void ComboBox::commit(std::size_t index) {
    if (!popupOpen() || index>=items_.size() || !items_[index].enabled) return;
    const auto item=items_[index]; const auto callback=onChange_;
    const bool changed=selected_!=item.id;
    selected_=item.id; caption(); closePopup();
    if (changed && callback) callback(item);
}
void ComboBox::search(char32_t codepoint) {
    if (codepoint<32 || (codepoint>=127 && codepoint<160)) return;
    const auto now=clock_();
    if (now<lastInput_ || now-lastInput_>=std::chrono::milliseconds(750)) prefix_.clear();
    lastInput_=now;
    std::string encoded; appendUtf8(encoded,codepoint); prefix_+=decodeUtf8(encoded);
    const auto fold=[](char32_t c){return c>=U'A' && c<=U'Z' ? c+32 : c;};
    for (std::size_t i=0;i<items_.size();++i) {
        if (!items_[i].enabled) continue;
        const auto label=decodeUtf8(items_[i].label);
        if (label.size()>=prefix_.size() && std::equal(prefix_.begin(),prefix_.end(),label.begin(),[&](auto a,auto b){return fold(a)==fold(b);})) { highlight(i); return; }
    }
}
bool ComboBox::onEvent(const Event& event,UIContext& context) {
    if (popupOpen()) {
        if (const auto* text=event.getIf<TextInputEvent>()) { search(text->codepoint); return false; }
        if (const auto* key=event.getIf<KeyEvent>()) {
            if (key->action!=KeyAction::Release) {
                if (key->key==Key::Enter && key->action==KeyAction::Press && highlighted_) { commit(*highlighted_); return true; }
                if (key->key==Key::Home || key->key==Key::End || key->key==Key::Up || key->key==Key::Down) {
                    const bool forward=key->key==Key::Home || key->key==Key::Down;
                    int i=key->key==Key::Home ? 0 : key->key==Key::End ? static_cast<int>(items_.size())-1 : static_cast<int>(*highlighted_)+(forward ? 1 : -1);
                    for (;i>=0 && i<static_cast<int>(items_.size());i+=forward ? 1 : -1)
                        if (items_[static_cast<std::size_t>(i)].enabled) { highlight(static_cast<std::size_t>(i)); break; }
                }
            }
            return false;
        }
    } else if (const auto* key=event.getIf<KeyEvent>(); key && focused() && key->action==KeyAction::Press
        && !key->mods.control && !key->mods.alt && !key->mods.super
        && (key->key==Key::Enter || key->key==Key::Space || key->key==Key::Down)) { openPopup(); return true; }
    return Button::onEvent(event,context);
}
void ComboBox::draw(Renderer& renderer) const {
    if (!visible()) return;
    const auto b=bounds(); const auto& t=theme();
    renderer.drawRoundedRect(b,t.cornerRadius,!enabled() ? t.disabled : hovered() || popupOpen() ? t.accentHovered : t.track);
    renderer.drawBorder(b,focused() ? t.focusWidth : 1,focused() ? t.focus : t.border,t.cornerRadius);
    drawText(renderer,!enabled() ? t.disabledText : selected_ ? t.text : t.mutedText,{b.x+8,b.y,std::max(0.0f,b.width-32),b.height});
    const float x=b.x+b.width-14,y=b.y+b.height/2;
    renderer.drawLine({x-4,y-2},{x,y+2},1.5f,t.text); renderer.drawLine({x,y+2},{x+4,y-2},1.5f,t.text);
}
}
