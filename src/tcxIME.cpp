#include "tcxIME.h"

void tcxIMEBase::enable() {
    if (enabled_) return;

    enabled_ = true;
    keyListener_ = events().keyPressed.listen([this](KeyEventArgs& e) {
        onKeyPressed(e);
    });

    startIMEObserver();
#ifdef __APPLE__
    setupIMEInputView();
#endif
}

void tcxIMEBase::disable() {
    if (!enabled_) return;

    enabled_ = false;
    keyListener_.disconnect();

    stopIMEObserver();
#ifdef __APPLE__
    removeIMEInputView();
#endif
}

void tcxIMEBase::clear() {
    markedText_ = U"";
    markedSelectedLocation_ = 0;
    markedSelectedLength_ = 0;
    lines_.clear();
    lines_.push_back(U"");
    candidates_.clear();
    candidateSelectedIndex_ = 0;
    cursorBlinkOffsetTime_ = getElapsedTimef();
    cursorLine_ = cursorPos_ = 0;
    selectCancel();

    if (state_ == Composing) {
        state_ = Kana;
    }
}

void tcxIMEBase::onKeyPressed(KeyEventArgs& key) {
    // Modifier key handling (Cmd on macOS, Ctrl elsewhere)
#ifdef __APPLE__
    bool isCtrl = key.super;
#else
    bool isCtrl = key.ctrl;
#endif

    if (isCtrl) {
        switch (key.key) {
        case 'c':
            // TODO: copy
            break;
        case 'v': {
            // Paste
            string clip = getClipboardString();
            u32string u32clip = UTF8toUTF32(clip);
            for (auto c : u32clip) {
                if (c == U'\n') newLine();
                else {
                    u32string s(1, c);
                    addStr(lines_[cursorLine_], s, cursorPos_);
                }
            }
            break;
        }
        case 'a':
            selectAll();
            break;
        default:
            break;
        }
        return;
    }

    // If IME has marked text, let OS handle it
    if (markedText_.length() > 0) {
        return;
    }

    // Handle confirmed text operations
    switch (key.key) {
    case KEY_ESCAPE:
        break;

    case KEY_BACKSPACE:
        deleteSelected();
        backspaceCharacter(lines_[cursorLine_], cursorPos_, true);
        break;

    case KEY_DELETE:
        deleteSelected();
        deleteCharacter(lines_[cursorLine_], cursorPos_, true);
        break;

    case KEY_ENTER:
        newLine();
        break;

    case KEY_UP:
        lineChange(-1);
        break;

    case KEY_DOWN:
        lineChange(1);
        break;

    case KEY_LEFT:
        if (cursorPos_ > 0) {
            cursorPos_--;
        }
        break;

    case KEY_RIGHT:
        cursorPos_++;
        if (cursorPos_ > (int)lines_[cursorLine_].length()) {
            cursorPos_ = (int)lines_[cursorLine_].length();
        }
        break;

    default:
        break;
    }

    cursorBlinkOffsetTime_ = getElapsedTimef();
}

string tcxIMEBase::getString() {
    string all;
    for (auto& a : lines_) {
        all += UTF32toUTF8(a);
        if (&a != &lines_.back()) {
            all += '\n';
        }
    }
    return all;
}

void tcxIMEBase::setString(const string& str) {
    clear();
    u32string u32str = UTF8toUTF32(str);
    insertText(u32str);
}

u32string tcxIMEBase::getU32String() {
    u32string all;
    for (auto& a : lines_) {
        all += a;
        if (&a != &lines_.back()) {
            all += U'\n';
        }
    }
    return all;
}

string tcxIMEBase::getLine(int l) {
    if (0 <= l && l < (int)lines_.size()) {
        return UTF32toUTF8(lines_[l]);
    }
    return "";
}

string tcxIMEBase::getLineSubstr(int l, int begin, int end) {
    if (0 <= l && l < (int)lines_.size()) {
        return UTF32toUTF8(lines_[l].substr(begin, end));
    }
    return "";
}

string tcxIMEBase::getMarkedText() {
    return UTF32toUTF8(markedText_);
}

string tcxIMEBase::getMarkedTextSubstr(int begin, int end) {
    return UTF32toUTF8(markedText_.substr(begin, end));
}

void tcxIMEBase::insertText(const u32string& str) {
    markedText_ = U"";
    markedSelectedLocation_ = 0;
    markedSelectedLength_ = 0;
    candidates_.clear();
    candidateSelectedIndex_ = 0;

    for (auto c : str) {
        if (c == U'\n' || c == U'\r') {
            newLine();
        } else {
            u32string s(1, c);
            addStr(lines_[cursorLine_], s, cursorPos_);
        }
    }

    state_ = (state_ == Composing) ? Kana : state_;
}

void tcxIMEBase::setMarkedTextFromOS(const u32string& str, int selectedLocation, int selectedLength) {
    markedText_ = str;
    markedSelectedLocation_ = selectedLocation;
    markedSelectedLength_ = selectedLength;

    state_ = (str.length() > 0) ? Composing : Kana;
}

void tcxIMEBase::unmarkText() {
    if (markedText_.length() > 0) {
        addStr(lines_[cursorLine_], markedText_, cursorPos_);
        markedText_ = U"";
        markedSelectedLocation_ = 0;
        markedSelectedLength_ = 0;
    }
    candidates_.clear();
    candidateSelectedIndex_ = 0;
    state_ = Kana;
}

void tcxIMEBase::setCandidates(const vector<u32string>& cands, int selectedIndex) {
    candidates_ = cands;
    candidateSelectedIndex_ = selectedIndex;
}

void tcxIMEBase::clearCandidates() {
    candidates_.clear();
    candidateSelectedIndex_ = 0;
}

void tcxIMEBase::deleteSelected() {
    if (!isSelected()) return;

    int bl, bn, el, en;
    tie(bl, bn) = selectBegin_;
    tie(el, en) = selectEnd_;

    if (bl > el || (bl == el && bn > en)) {
        tie(el, en) = selectBegin_;
        tie(bl, bn) = selectEnd_;
    }

    int blen = (int)lines_[bl].length();
    if (blen < bn) bn = blen;

    int elen = (int)lines_[el].length();
    if (elen < en) en = elen;

    lines_[bl] = lines_[bl].substr(0, bn) + lines_[el].substr(en, elen - en);

    int delNum = el - bl;
    for (int i = 0; i < delNum; ++i) {
        lines_.erase(lines_.begin() + bl + 1);
    }

    cursorLine_ = bl;
    cursorPos_ = bn;
    selectCancel();
}

void tcxIMEBase::newLine() {
    lines_.insert(lines_.begin() + cursorLine_ + 1, U"");
    lines_[cursorLine_ + 1] = lines_[cursorLine_].substr(cursorPos_, lines_[cursorLine_].length() - cursorPos_);
    lines_[cursorLine_] = lines_[cursorLine_].substr(0, cursorPos_);

    cursorLine_++;
    cursorPos_ = 0;
}

void tcxIMEBase::lineChange(int n) {
    if (n == 0) return;
    cursorLine_ = max(0, min(cursorLine_ + n, (int)lines_.size() - 1));
    if (cursorPos_ > (int)lines_[cursorLine_].size()) {
        cursorPos_ = (int)lines_[cursorLine_].length();
    }
}

void tcxIMEBase::addStr(u32string& target, const u32string& str, int& p) {
    target = target.substr(0, p) + str + target.substr(p, target.length() - p);
    p += str.length();
}

void tcxIMEBase::backspaceCharacter(u32string& str, int& pos, bool lineMerge) {
    if (pos == 0) {
        if (lineMerge && cursorLine_ > 0) {
            cursorPos_ = (int)lines_[cursorLine_ - 1].length();
            lines_[cursorLine_ - 1] += lines_[cursorLine_];
            lines_.erase(lines_.begin() + cursorLine_);
            cursorLine_--;
        }
    } else {
        if ((int)str.length() < pos) pos = (int)str.length();
        str = str.substr(0, pos - 1) + str.substr(pos, str.length() - pos);
        pos--;
    }
}

void tcxIMEBase::deleteCharacter(u32string& str, int& pos, bool lineMerge) {
    if ((int)str.length() < pos) {
        pos = (int)str.length();
    }
    if ((int)str.length() == pos) {
        if (lineMerge && cursorLine_ + 1 < (int)lines_.size()) {
            lines_[cursorLine_] += lines_[cursorLine_ + 1];
            lines_.erase(lines_.begin() + cursorLine_ + 1);
        }
    } else {
        str = str.substr(0, pos) + str.substr(pos + 1, str.length() - pos - 1);
    }
}

// UTF-32 to UTF-8 conversion
string tcxIMEBase::UTF32toUTF8(const u32string& u32str) {
    string result;
    for (char32_t c : u32str) {
        if (c < 0x80) {
            result += static_cast<char>(c);
        } else if (c < 0x800) {
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            result += static_cast<char>(0xE0 | (c >> 12));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (c >> 18));
            result += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return result;
}

string tcxIMEBase::UTF32toUTF8(const char32_t& u32char) {
    return UTF32toUTF8(u32string(1, u32char));
}

// UTF-8 to UTF-32 conversion
u32string tcxIMEBase::UTF8toUTF32(const string& str) {
    u32string result;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = str[i];
        char32_t cp;
        if ((c & 0x80) == 0) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = (c & 0x1F) << 6;
            cp |= (str[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = (c & 0x0F) << 12;
            cp |= (str[i + 1] & 0x3F) << 6;
            cp |= (str[i + 2] & 0x3F);
            i += 3;
        } else {
            cp = (c & 0x07) << 18;
            cp |= (str[i + 1] & 0x3F) << 12;
            cp |= (str[i + 2] & 0x3F) << 6;
            cp |= (str[i + 3] & 0x3F);
            i += 4;
        }
        result += cp;
    }
    return result;
}
