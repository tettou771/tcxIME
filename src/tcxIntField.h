#pragma once

// =============================================================================
// tcxIME::IntegerField_<T> - Integer input widget
// Defined as a nested class of tcxIME (included via tcxIME.h)
//
// Usage:
//   tcxIME::IntField   → IntegerField_<int>
//   tcxIME::Int64Field → IntegerField_<int64_t>
// =============================================================================

#include <limits>
#include <cstdlib>

template<typename T>
class tcxIME::IntegerField_ : public tc::RectNode {
public:
    // Value access
    T getValue() const { return value_; }
    int getAsInt() const { return static_cast<int>(value_); }
    int64_t getAsInt64() const { return static_cast<int64_t>(value_); }

    void setValue(T val) {
        value_ = clamp(val);
        updateDisplay();
    }

    // Range
    void setRange(T min, T max) { min_ = min; max_ = max; clampAndUpdate(); }
    void setMin(T min) { min_ = min; clampAndUpdate(); }
    void setMax(T max) { max_ = max; clampAndUpdate(); }
    void clearMin() { min_ = std::numeric_limits<T>::lowest(); }
    void clearMax() { max_ = std::numeric_limits<T>::max(); }
    void clearRange() { clearMin(); clearMax(); }
    T getMin() const { return min_; }
    T getMax() const { return max_; }

    // Value confirmed callback
    std::function<void(T)> onValueChanged;

    // Font setup
    void setFont(std::string path, float fontSize) { ime_.setFont(path, fontSize); }
    void setFont(tc::Font* sharedFont) { ime_.setFont(sharedFont); }

    // Access underlying IME
    tcxIME& getIME() { return ime_; }

    void setup() override {
        enableEvents();
        ime_.setEnableNewLine(false);
        ime_.setMaxWidth(getWidth());

        // Filter: only allow digits and minus sign
        ime_.inputFilter = [](char32_t c) -> bool {
            return (c >= '0' && c <= '9') || c == '-';
        };

        // On Enter: validate and confirm
        ime_.onEnter = [this]() {
            confirmValue();
        };

        updateDisplay();
    }

    void setSize(float w, float h) override {
        tc::RectNode::setSize(w, h);
        ime_.setMaxWidth(w);
    }

    void enable() { ime_.enable(); tc::redraw(); }
    void disable() { ime_.disable(); tc::redraw(); }
    bool isEnabled() { return ime_.isEnabled(); }

    void draw() override {
        ime_.draw(0, 0);
    }

    bool onMousePress(tc::Vec2 local, int button) override {
        (void)button;
        activatingClick_ = !ime_.isEnabled();
        if (activatingClick_) enable();
        ime_.setCursorFromMouse(local.x, local.y);
        tc::redraw();
        return true;
    }

    bool onMouseDrag(tc::Vec2 local, int button) override {
        (void)button;
        if (activatingClick_) return true;
        ime_.extendSelectionFromMouse(local.x, local.y);
        tc::redraw();
        return true;
    }

private:
    tcxIME ime_;
    bool activatingClick_ = false;
    T value_ = 0;
    T min_ = std::numeric_limits<T>::lowest();
    T max_ = std::numeric_limits<T>::max();

    T clamp(T val) const {
        if (val < min_) return min_;
        if (val > max_) return max_;
        return val;
    }

    void clampAndUpdate() {
        T clamped = clamp(value_);
        if (clamped != value_) {
            value_ = clamped;
            updateDisplay();
        }
    }

    void updateDisplay() {
        ime_.setString(std::to_string(value_));
    }

    void confirmValue() {
        std::string str = ime_.getString();
        if (str.empty()) {
            setValue(0);
        } else {
            try {
                T parsed;
                if constexpr (std::is_same_v<T, int>) {
                    parsed = static_cast<T>(std::stoi(str));
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    parsed = static_cast<T>(std::stoll(str));
                } else {
                    parsed = static_cast<T>(std::stol(str));
                }
                value_ = clamp(parsed);
            } catch (...) {
                // Invalid input — revert to current value
            }
        }
        updateDisplay();
        if (onValueChanged) onValueChanged(value_);
        tc::redraw();
    }
};
