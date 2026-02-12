#pragma once

// =============================================================================
// tcxIME - Native OS IME (Input Method Editor) support for TrussC
// Enables Japanese/CJK text input with kanji conversion
// =============================================================================

#include <string>
#include <vector>
#include <tuple>

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

#include <TrussC.h>
using namespace std;
using namespace tc;

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

    // Text access (u32string internally, UTF-8 for getString)
    string getString();
    void setString(const string& str);
    u32string getU32String();
    string getLine(int l);
    string getLineSubstr(int l, int begin, int end);
    string getMarkedText();
    string getMarkedTextSubstr(int begin, int end);

    // Called from OS (via tcxIMEView callbacks)
    void insertText(const u32string& str);
    void setMarkedTextFromOS(const u32string& str, int selectedLocation, int selectedLength);
    void unmarkText();

    // Conversion candidates (from OS)
    void setCandidates(const vector<u32string>& cands, int selectedIndex);
    void clearCandidates();

    // Screen position of marked text (for IME candidate window)
    virtual Vec2 getMarkedTextScreenPosition() { return lastDrawPos_; }

    // UTF conversion utilities
    static string UTF32toUTF8(const u32string& u32str);
    static string UTF32toUTF8(const char32_t& u32char);
    static u32string UTF8toUTF32(const string& str);

protected:
    bool enabled_ = false;
    Vec2 lastDrawPos_;

    // Marked text (composing)
    u32string markedText_;
    int markedSelectedLocation_ = 0;
    int markedSelectedLength_ = 0;

    // Conversion candidates
    vector<u32string> candidates_;
    int candidateSelectedIndex_ = 0;

    // Confirmed text (multi-line)
    vector<u32string> lines_;

    // Selection
    typedef tuple<int, int> TextSelectPos;
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

    void newLine();
    void lineChange(int n);

    // Cursor
    int cursorLine_ = 0;
    int cursorPos_ = 0;

    // String manipulation
    void addStr(u32string& target, const u32string& str, int& p);
    void backspaceCharacter(u32string& str, int& pos, bool lineMerge = false);
    void deleteCharacter(u32string& str, int& pos, bool lineMerge = false);

    enum State { Eisu, Kana, Composing };
    State state_;

    // OS IME observer
    void startIMEObserver();
    void stopIMEObserver();
    void syncWithSystemIME();

    // Key handler
    void onKeyPressed(KeyEventArgs& key);
    EventListener keyListener_;

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
    // Set font from file path
    void setFont(string path, float fontSize) {
        font_.load(path, fontSize);
        fontPtr_ = nullptr;
    }

    // Share an existing font
    void setFont(Font* sharedFont) {
        fontPtr_ = sharedFont;
    }

    // Draw (confirmed text + composing text + cursor)
    void draw(float x, float y) {
        Font& f = getFont();
        if (!f.isLoaded()) {
            logError() << "tcxIME: font is not loaded.";
            return;
        }

        lastDrawPos_ = Vec2(x, y);

        float fontSize = f.getSize();
        float lineHeight = f.getLineHeight();
        float ascent = f.getAscent();
        float margin = fontSize * 0.1f;
        // Underline Y position: just below baseline (baseline = ascent from top)
        float underlineY = ascent + fontSize * 0.15f;

        // Cursor drawing lambda (2px wide rect, from top to baseline)
        auto drawCursor = [&](float cx, float cy) {
            if (!enabled_) return;
            if (fmod(getElapsedTimef() - cursorBlinkOffsetTime_, 0.8f) < 0.4f) {
                tc::drawRect(cx, cy, 2, ascent);
            }
        };

        pushMatrix();
        translate(x, y);

        for (int i = 0; i < (int)lines_.size(); ++i) {
            if (i != cursorLine_) {
                // Non-active line
                f.drawString(UTF32toUTF8(lines_[i]), 0, 0);
            } else {
                // Current input line
                pushMatrix();

                // Confirmed text before cursor
                string beforeCursor = getLineSubstr(cursorLine_, 0, cursorPos_);
                f.drawString(beforeCursor, 0, 0);
                float beforeW = f.stringWidth(beforeCursor);

                translate(beforeW, 0);

                if (markedText_.length() > 0) {
                    translate(margin, 0);

                    string markedStr = getMarkedText();
                    f.drawString(markedStr, 0, 0);
                    float markedW = f.stringWidth(markedStr);

                    // Underlines for marked text segments
                    string selStart = getMarkedTextSubstr(0, markedSelectedLocation_);
                    float selStartW = f.stringWidth(selStart);

                    if (markedSelectedLength_ > 0) {
                        string selText = getMarkedTextSubstr(markedSelectedLocation_, markedSelectedLength_);
                        float selW = f.stringWidth(selText);

                        // Thin underline before selection (1px rect)
                        if (selStartW > 0) {
                            tc::drawRect(1, underlineY, selStartW - 2, 1);
                        }

                        // Thick underline for selected part (3px rect)
                        tc::drawRect(selStartW + 1, underlineY - 1, selW - 2, 3);

                        // Thin underline after selection
                        if (selStartW + selW < markedW) {
                            tc::drawRect(selStartW + selW + 1, underlineY, markedW - selStartW - selW - 2, 1);
                        }
                    } else {
                        // No selection, thin underline for entire marked text
                        tc::drawRect(1, underlineY, markedW - 2, 1);
                    }

                    // Conversion candidates
                    if (candidates_.size() > 0) {
                        float lh = f.getLineHeight();
                        pushMatrix();
                        translate(0, lh);

                        for (int j = 0; j < (int)candidates_.size(); ++j) {
                            string candStr = UTF32toUTF8(candidates_[j]);

                            if (j == candidateSelectedIndex_) {
                                pushStyle();
                                float candW = f.stringWidth(candStr);
                                setColor(0.4f, 0.4f, 0.4f, 0.6f);
                                tc::drawRect(-2, 0, candW + 4, lh);
                                popStyle();
                            }

                            f.drawString(candStr, 0, 0);
                            translate(0, lh);
                        }
                        popMatrix();
                    }

                    translate(markedW + margin, 0);
                } else {
                    // No marked text - draw cursor
                    drawCursor(0, 0);
                }

                // Confirmed text after cursor
                string afterCursor = getLineSubstr(cursorLine_, cursorPos_, (int)lines_[cursorLine_].length() - cursorPos_);
                f.drawString(afterCursor, 0, 0);

                popMatrix();
            }

            translate(0, lineHeight);
        }

        popMatrix();
    }

    void draw(Vec2 pos) {
        draw(pos.x, pos.y);
    }

    // Screen position of marked text (for IME candidate window)
    Vec2 getMarkedTextScreenPosition() override {
        Font& f = getFont();
        float x = lastDrawPos_.x;
        float y = lastDrawPos_.y;

        y += f.getLineHeight() * cursorLine_;

        string beforeCursor = getLineSubstr(cursorLine_, 0, cursorPos_);
        x += f.stringWidth(beforeCursor);

        return Vec2(x, y);
    }

private:
    Font font_;
    Font* fontPtr_ = nullptr;
    Font& getFont() { return fontPtr_ ? *fontPtr_ : font_; }
};
