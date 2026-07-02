#pragma once

// =============================================================================
// tcx::ime::IME::TextField - Node-based text input with built-in mouse handling
// Defined as a nested class of IME (included via tcxIME.h)
// =============================================================================

namespace tcx { namespace ime {

class IME::TextField : public tc::RectNode {
public:
    // Font setup
    void setFont(std::string path, float fontSize) { ime_.setFont(path, fontSize); }
    void setFont(tc::Font* sharedFont) { ime_.setFont(sharedFont); }

    // Text access
    std::string getString() { return ime_.getString(); }
    void setString(const std::string& str) { ime_.setString(str); }
    std::string getMarkedText() { return ime_.getMarkedText(); }
    void clear() { ime_.clear(); }

    // IME state
    bool isJapaneseMode() { return ime_.isJapaneseMode(); }

    // Max width for wrapping (also updates via setSize)
    void setMaxWidth(float w) { ime_.setMaxWidth(w); }

    void setSize(float w, float h) override {
        tc::RectNode::setSize(w, h);
        ime_.setMaxWidth(w);
    }

    // Newline mode (default: enabled)
    void setEnableNewLine(bool b) { ime_.setEnableNewLine(b); }
    bool getEnableNewLine() { return ime_.getEnableNewLine(); }

    // Input filter: set a character filter function
    // Example: field->setInputFilter([](char32_t c) { return c < 128; }); // ASCII only
    void setInputFilter(std::function<bool(char32_t)> filter) {
        ime_.inputFilter = filter;
    }

    // Access underlying IME
    IME& getIME() { return ime_; }

    void setup() override {
        enableEvents();
    }

    void enable() { ime_.enable(); tc::redraw(); }
    void disable() { ime_.disable(); tc::redraw(); }
    bool isEnabled() { return ime_.isEnabled(); }

    void draw() override {
        ime_.draw(0, 0);
    }

    // Hit test: only where text actually exists (jagged for multi-line)
    bool hitTest(tc::Vec2 local) override {
        if (!isEventsEnabled()) return false;

        float lineHeight = ime_.getLineHeight();
        if (lineHeight <= 0 || local.y < 0) return false;

        int line = (int)(local.y / lineHeight);
        if (line >= ime_.getLineCount()) return false;

        // Width of this line's text + cursor width + small padding
        float lineW = ime_.getLineWidth(line) + 4;
        return local.x >= 0 && local.x <= lineW;
    }

    bool onMousePress(const tc::MouseEventArgs& e) override {
        activatingClick_ = !ime_.isEnabled();
        if (activatingClick_) enable();
        ime_.setCursorFromMouse(e.pos.x, e.pos.y);
        tc::redraw();
        return true;
    }

    bool onMouseDrag(const tc::MouseDragEventArgs& e) override {
        if (activatingClick_) return true;
        ime_.extendSelectionFromMouse(e.pos.x, e.pos.y);
        tc::redraw();
        return true;
    }

private:
    IME ime_;
    bool activatingClick_ = false;
};

} } // namespace tcx::ime
