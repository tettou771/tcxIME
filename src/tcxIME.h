#pragma once

// =============================================================================
// tcxIME - Native OS IME (Input Method Editor) support for TrussC
// Enables Japanese/CJK text input with kanji conversion
//
// Include this single header to get everything:
//   tcxIMEBase   - Platform-agnostic text management
//   tcxIME       - Font-based rendering
//   tcxIME::TextField       - RectNode text input widget
//   tcxIME::FloatField      - RectNode float input widget
//   tcxIME::IntField        - RectNode int input widget
// =============================================================================

#include "tcxIMEBase.h"

// ---------------------------------------------------------------------------
// tcxIME - Main class with Font-based drawing
// ---------------------------------------------------------------------------
class tcxIME : public tcxIMEBase {
public:
    // Nested widget classes (defined in separate headers, included below)
    class TextField;
    template<typename T> class FloatingPointField_;
    template<typename T> class IntegerField_;
    using FloatField  = FloatingPointField_<float>;
    using DoubleField = FloatingPointField_<double>;
    using IntField    = IntegerField_<int>;
    using Int64Field  = IntegerField_<int64_t>;

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

// Include nested class definitions
#include "tcxTextField.h"
#include "tcxFloatField.h"
#include "tcxIntField.h"
