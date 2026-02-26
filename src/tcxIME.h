#pragma once

// =============================================================================
// tcxIME - Native OS IME (Input Method Editor) support for TrussC
// Enables Japanese/CJK text input with kanji conversion
// =============================================================================

#include <string>
#include <functional>
#include <vector>
#include <tuple>
#include <set>

#ifdef __APPLE__
// Forward-declare CoreFoundation types to avoid Carbon.h
// (Carbon.h pulls in QuickDraw which defines a global 'Rect' that
//  conflicts with trussc::Rect when 'using namespace tc' is active)
typedef struct __CFNotificationCenter * CFNotificationCenterRef;
typedef const struct __CFString * CFStringRef;
typedef CFStringRef CFNotificationName;
typedef const struct __CFDictionary * CFDictionaryRef;
#endif

#include <TrussC.h>

// ---------------------------------------------------------------------------
// tcxIMEBase - Platform-agnostic IME text management
// ---------------------------------------------------------------------------
class tcxIMEBase {
public:
    tcxIMEBase() {
        state_ = Eisu;
        clear();
    }
    virtual ~tcxIMEBase() = default;

    void enable();
    void disable();
    void clear();

    bool isEnabled() { return enabled_; }
    bool isJapaneseMode() { return state_ == Kana || state_ == Composing; }

    // Newline mode: when enabled (default), Enter inserts a newline
    void setEnableNewLine(bool b) { enableNewLine_ = b; }
    bool getEnableNewLine() { return enableNewLine_; }

    // Enter key callback (called after newline if newline is enabled)
    std::function<void()> onEnter;

    // Text access (u32string internally, UTF-8 for getString)
    std::string getString();
    void setString(const std::string& str);
    std::u32string getU32String();
    std::string getLine(int l);
    std::string getLineSubstr(int l, int begin, int end);
    std::string getMarkedText();
    std::string getMarkedTextSubstr(int begin, int end);

    // Called from OS (via tcxIMEView callbacks)
    void insertText(const std::u32string& str);
    void setMarkedTextFromOS(const std::u32string& str, int selectedLocation, int selectedLength);
    void unmarkText();

    // Conversion candidates (from OS)
    void setCandidates(const std::vector<std::u32string>& cands, int selectedIndex);
    void clearCandidates();

    // Screen position of marked text (for IME candidate window)
    virtual tc::Vec2 getMarkedTextScreenPosition() { return lastDrawPos_; }

    // UTF conversion utilities
    static std::string UTF32toUTF8(const std::u32string& u32str);
    static std::string UTF32toUTF8(const char32_t& u32char);
    static std::u32string UTF8toUTF32(const std::string& str);

protected:
    bool enabled_ = false;
    bool enableNewLine_ = true;
    tc::Vec2 lastDrawPos_;

    // Marked text (composing)
    std::u32string markedText_;
    int markedSelectedLocation_ = 0;
    int markedSelectedLength_ = 0;

    // Conversion candidates
    std::vector<std::u32string> candidates_;
    int candidateSelectedIndex_ = 0;

    // Confirmed text (multi-line)
    std::vector<std::u32string> lines_;
    std::set<int> softBreaks_;  // indices of soft-wrapped lines (auto line breaks)
    virtual void rewrap() {}    // overridden by tcxIME when maxWidth is set

    // Selection
    typedef std::tuple<int, int> TextSelectPos;
    TextSelectPos selectBegin_, selectEnd_;
    void selectCancel() {
        selectBegin_ = selectEnd_ = TextSelectPos(0, 0);
    }
    bool isSelected() {
        return selectBegin_ != selectEnd_;
    }
    void selectAll() {
        selectBegin_ = TextSelectPos(0, 0);
        selectEnd_ = TextSelectPos(lines_.size() - 1, lines_.back().length());
    }
    void deleteSelected();
    std::string getSelectedText();

    void newLine();
    void lineChange(int n);

    // Cursor
    int cursorLine_ = 0;
    int cursorPos_ = 0;

    // String manipulation
    void addStr(std::u32string& target, const std::u32string& str, int& p);
    void backspaceCharacter(std::u32string& str, int& pos, bool lineMerge = false);
    void deleteCharacter(std::u32string& str, int& pos, bool lineMerge = false);

    enum State { Eisu, Kana, Composing };
    State state_;

    // OS IME observer
    void startIMEObserver();
    void stopIMEObserver();
    void syncWithSystemIME();

    // Key handler
    void onKeyPressed(tc::KeyEventArgs& key);
    tc::EventListener keyListener_;

#ifdef __APPLE__
    static void onInputSourceChanged(CFNotificationCenterRef center,
                                     void* observer,
                                     CFNotificationName name,
                                     const void* object,
                                     CFDictionaryRef userInfo);

    // Shared IME view (one per window)
    static void* sharedIMEView_;
    static void* sharedOriginalContentView_;
    static tcxIMEBase* activeIMEInstance_;
    static int imeViewRefCount_;

    void setupIMEInputView();
    void removeIMEInputView();
    void becomeActiveIME();
#endif

    float cursorBlinkOffsetTime_ = 0;
};

// ---------------------------------------------------------------------------
// tcxIME - Main class with Font-based drawing
// ---------------------------------------------------------------------------
class tcxIME : public tcxIMEBase {
public:
    class TextField;  // Node-based wrapper with built-in mouse handling

    // Set font from file path
    void setFont(std::string path, float fontSize) {
        font_.load(path, fontSize);
        fontPtr_ = nullptr;
    }

    // Share an existing font
    void setFont(tc::Font* sharedFont) {
        fontPtr_ = sharedFont;
    }

    // Set max width for automatic word wrapping (0 = no wrapping)
    void setMaxWidth(float w) {
        maxWidth_ = w;
        rewrap();
    }

    // Draw (confirmed text + composing text + cursor)
    void draw(float x, float y) {
        tc::Font& f = getFont();
        if (!f.isLoaded()) {
            tc::logError() << "tcxIME: font is not loaded.";
            return;
        }

        lastDrawPos_ = tc::Vec2(x, y);

        float fontSize = f.getSize();
        float lineHeight = f.getLineHeight();
        float ascent = f.getAscent();
        float margin = fontSize * 0.1f;
        // Underline Y position: just below baseline (baseline = ascent from top)
        float underlineY = ascent + fontSize * 0.15f;

        // Cursor drawing lambda (2px wide rect, from top to baseline)
        auto drawCursor = [&](float cx, float cy) {
            if (!enabled_) return;
            if (fmod(tc::getElapsedTimef() - cursorBlinkOffsetTime_, 1.f) < 0.5f) {
                tc::drawRect(cx, cy, 2, ascent);
            }
        };

        // Selection highlight lambda (blue semi-transparent background)
        auto drawSelectionHighlight = [&](int lineIdx, float lineX) {
            if (!isSelected()) return;
            int bl, bn, el, en;
            std::tie(bl, bn) = selectBegin_;
            std::tie(el, en) = selectEnd_;
            if (bl > el || (bl == el && bn > en)) {
                std::swap(bl, el);
                std::swap(bn, en);
            }
            if (lineIdx < bl || lineIdx > el) return;

            int lineLen = (int)lines_[lineIdx].length();
            int startChar = (lineIdx == bl) ? bn : 0;
            int endChar   = (lineIdx == el) ? en : lineLen;
            startChar = std::min(startChar, lineLen);
            endChar   = std::min(endChar, lineLen);
            if (startChar >= endChar) return;

            float sx = f.stringWidth(UTF32toUTF8(lines_[lineIdx].substr(0, startChar)));
            float ex = f.stringWidth(UTF32toUTF8(lines_[lineIdx].substr(0, endChar)));
            tc::pushStyle();
            tc::setColor(0.2f, 0.5f, 1.0f, 0.3f);
            tc::drawRect(lineX + sx, 0, ex - sx, lineHeight);
            tc::popStyle();
        };

        tc::pushMatrix();
        tc::translate(x, y);

        for (int i = 0; i < (int)lines_.size(); ++i) {
            if (i != cursorLine_) {
                // Non-active line
                drawSelectionHighlight(i, 0);
                f.drawString(UTF32toUTF8(lines_[i]), 0, 0);
            } else {
                // Current input line
                std::string beforeCursor = getLineSubstr(cursorLine_, 0, cursorPos_);
                float beforeW = f.stringWidth(beforeCursor);

                if (maxWidth_ > 0 && markedText_.length() > 0) {
                    // --- Wrapped marked text rendering ---
                    f.drawString(beforeCursor, 0, 0);

                    float segX = beforeW + margin;
                    float segY = 0;
                    int extraLines = 0;
                    int mPos = 0;
                    int mLen = (int)markedText_.length();

                    while (mPos < mLen) {
                        float availW = maxWidth_ - segX;
                        if (availW <= 0) {
                            segY += lineHeight;
                            segX = 0;
                            extraLines++;
                            availW = maxWidth_;
                        }

                        // Find break point
                        int bk = mPos;
                        for (int j = mPos + 1; j <= mLen; j++) {
                            std::string sub = UTF32toUTF8(markedText_.substr(mPos, j - mPos));
                            if (f.stringWidth(sub) > availW) break;
                            bk = j;
                        }
                        if (bk == mPos) bk = mPos + 1;

                        // Draw segment
                        std::string segStr = UTF32toUTF8(markedText_.substr(mPos, bk - mPos));
                        float segW = f.stringWidth(segStr);
                        f.drawString(segStr, segX, segY);

                        // Underlines for this segment
                        float ulY = segY + underlineY;
                        if (markedSelectedLength_ > 0) {
                            int sS = markedSelectedLocation_;
                            int sE = sS + markedSelectedLength_;
                            int oS = mPos > sS ? mPos : sS;
                            int oE = bk < sE ? bk : sE;

                            if (oS < oE) {
                                // Thin underline before selection
                                if (mPos < oS) {
                                    float w = f.stringWidth(UTF32toUTF8(markedText_.substr(mPos, oS - mPos)));
                                    if (w > 0) tc::drawRect(segX + 1, ulY, w - 2, 1);
                                }
                                // Thick underline for selection
                                float offX = (oS > mPos) ? f.stringWidth(UTF32toUTF8(markedText_.substr(mPos, oS - mPos))) : 0;
                                float selW = f.stringWidth(UTF32toUTF8(markedText_.substr(oS, oE - oS)));
                                tc::drawRect(segX + offX + 1, ulY - 1, selW - 2, 3);
                                // Thin underline after selection
                                if (oE < bk) {
                                    float offX2 = offX + selW;
                                    float w = f.stringWidth(UTF32toUTF8(markedText_.substr(oE, bk - oE)));
                                    if (w > 0) tc::drawRect(segX + offX2 + 1, ulY, w - 2, 1);
                                }
                            } else {
                                if (segW > 0) tc::drawRect(segX + 1, ulY, segW - 2, 1);
                            }
                        } else {
                            if (segW > 0) tc::drawRect(segX + 1, ulY, segW - 2, 1);
                        }

                        segX += segW;
                        mPos = bk;
                    }

                    // Conversion candidates
                    if (candidates_.size() > 0) {
                        float lh = f.getLineHeight();
                        float candX = beforeW + margin;
                        float candY = lh;
                        for (int j = 0; j < (int)candidates_.size(); ++j) {
                            std::string candStr = UTF32toUTF8(candidates_[j]);
                            if (j == candidateSelectedIndex_) {
                                tc::pushStyle();
                                float candW = f.stringWidth(candStr);
                                tc::setColor(0.4f, 0.4f, 0.4f, 0.6f);
                                tc::drawRect(candX - 2, candY, candW + 4, lh);
                                tc::popStyle();
                            }
                            f.drawString(candStr, candX, candY);
                            candY += lh;
                        }
                    }

                    // After cursor text
                    std::string afterCursor = getLineSubstr(cursorLine_, cursorPos_, (int)lines_[cursorLine_].length() - cursorPos_);
                    f.drawString(afterCursor, segX + margin, segY);

                    tc::translate(0, lineHeight * extraLines);

                } else {
                    // --- Original path (no wrapping or no marked text) ---
                    drawSelectionHighlight(i, 0);
                    tc::pushMatrix();

                    f.drawString(beforeCursor, 0, 0);
                    tc::translate(beforeW, 0);

                    if (markedText_.length() > 0) {
                        tc::translate(margin, 0);

                        std::string markedStr = getMarkedText();
                        f.drawString(markedStr, 0, 0);
                        float markedW = f.stringWidth(markedStr);

                        std::string selStart = getMarkedTextSubstr(0, markedSelectedLocation_);
                        float selStartW = f.stringWidth(selStart);

                        if (markedSelectedLength_ > 0) {
                            std::string selText = getMarkedTextSubstr(markedSelectedLocation_, markedSelectedLength_);
                            float selW = f.stringWidth(selText);

                            if (selStartW > 0) {
                                tc::drawRect(1, underlineY, selStartW - 2, 1);
                            }
                            tc::drawRect(selStartW + 1, underlineY - 1, selW - 2, 3);
                            if (selStartW + selW < markedW) {
                                tc::drawRect(selStartW + selW + 1, underlineY, markedW - selStartW - selW - 2, 1);
                            }
                        } else {
                            tc::drawRect(1, underlineY, markedW - 2, 1);
                        }

                        if (candidates_.size() > 0) {
                            float lh = f.getLineHeight();
                            tc::pushMatrix();
                            tc::translate(0, lh);

                            for (int j = 0; j < (int)candidates_.size(); ++j) {
                                std::string candStr = UTF32toUTF8(candidates_[j]);
                                if (j == candidateSelectedIndex_) {
                                    tc::pushStyle();
                                    float candW = f.stringWidth(candStr);
                                    tc::setColor(0.4f, 0.4f, 0.4f, 0.6f);
                                    tc::drawRect(-2, 0, candW + 4, lh);
                                    tc::popStyle();
                                }
                                f.drawString(candStr, 0, 0);
                                tc::translate(0, lh);
                            }
                            tc::popMatrix();
                        }

                        tc::translate(markedW + margin, 0);
                    } else {
                        drawCursor(0, 0);
                    }

                    std::string afterCursor = getLineSubstr(cursorLine_, cursorPos_, (int)lines_[cursorLine_].length() - cursorPos_);
                    f.drawString(afterCursor, 0, 0);

                    tc::popMatrix();
                }
            }

            tc::translate(0, lineHeight);
        }

        tc::popMatrix();
    }

    void draw(tc::Vec2 pos) {
        draw(pos.x, pos.y);
    }

    // Set cursor position from mouse click (same coordinate space as draw())
    bool setCursorFromMouse(float mx, float my) {
        if (!hitTestMouse(mx, my)) return false;
        selectBegin_ = selectEnd_ = TextSelectPos(cursorLine_, cursorPos_);
        return true;
    }

    // Extend selection by dragging (call on mouseDragged)
    bool extendSelectionFromMouse(float mx, float my) {
        if (!hitTestMouse(mx, my)) return false;
        selectEnd_ = TextSelectPos(cursorLine_, cursorPos_);
        return true;
    }

    // Screen position of marked text (for IME candidate window)
    tc::Vec2 getMarkedTextScreenPosition() override {
        tc::Font& f = getFont();
        float x = lastDrawPos_.x;
        float y = lastDrawPos_.y;

        y += f.getLineHeight() * cursorLine_;

        std::string beforeCursor = getLineSubstr(cursorLine_, 0, cursorPos_);
        x += f.stringWidth(beforeCursor);

        return tc::Vec2(x, y);
    }

    // Get text line width (for hit testing by TextField)
    float getLineWidth(int line) {
        tc::Font& f = getFont();
        if (!f.isLoaded() || line < 0 || line >= (int)lines_.size()) return 0;
        float w = f.stringWidth(UTF32toUTF8(lines_[line]));
        // Include marked text width on cursor line
        if (line == cursorLine_ && markedText_.length() > 0) {
            w += f.stringWidth(UTF32toUTF8(markedText_)) + f.getSize() * 0.2f;
        }
        return w;
    }

    int getLineCount() { return (int)lines_.size(); }

    float getLineHeight() {
        tc::Font& f = getFont();
        return f.isLoaded() ? f.getLineHeight() : 0;
    }

private:
    tc::Font font_;
    tc::Font* fontPtr_ = nullptr;
    tc::Font& getFont() { return fontPtr_ ? *fontPtr_ : font_; }
    float maxWidth_ = 0;
    void rewrap() override;

    // Hit-test: convert mouse position to cursor line/pos
    bool hitTestMouse(float mx, float my) {
        tc::Font& f = getFont();
        if (!f.isLoaded()) return false;

        float lineHeight = f.getLineHeight();
        float localX = mx - lastDrawPos_.x;
        float localY = my - lastDrawPos_.y;
        if (localY < 0) localY = 0;

        int line = (int)(localY / lineHeight);
        if (line >= (int)lines_.size()) line = (int)lines_.size() - 1;
        if (line < 0) line = 0;

        int pos = 0;
        int len = (int)lines_[line].length();
        if (localX > 0) {
            float prevW = 0;
            for (int j = 1; j <= len; j++) {
                float w = f.stringWidth(getLineSubstr(line, 0, j));
                if (localX < (prevW + w) * 0.5f) { pos = j - 1; break; }
                prevW = w;
                pos = j;
            }
        }

        cursorLine_ = line;
        cursorPos_ = pos;
        cursorBlinkOffsetTime_ = tc::getElapsedTimef();
        return true;
    }
};

// ---------------------------------------------------------------------------
// tcxIME::TextField - Node-based text input with built-in mouse handling
// ---------------------------------------------------------------------------
class tcxIME::TextField : public tc::RectNode {
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

    // Max width for wrapping
    void setMaxWidth(float w) { ime_.setMaxWidth(w); }

    // Newline mode (default: enabled)
    void setEnableNewLine(bool b) { ime_.setEnableNewLine(b); }
    bool getEnableNewLine() { return ime_.getEnableNewLine(); }

    // Access underlying IME
    tcxIME& getIME() { return ime_; }

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

    bool onMousePress(tc::Vec2 local, int button) override {
        (void)button;
        if (!ime_.isEnabled()) enable();
        ime_.setCursorFromMouse(local.x, local.y);
        tc::redraw();
        return true;
    }

    bool onMouseDrag(tc::Vec2 local, int button) override {
        (void)button;
        ime_.extendSelectionFromMouse(local.x, local.y);
        tc::redraw();
        return true;
    }

private:
    tcxIME ime_;
};
