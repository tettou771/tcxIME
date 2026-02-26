#pragma once

// =============================================================================
// tcxIMEBase - Platform-agnostic IME text management
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

    // Input character filter (return true to accept, false to reject)
    // If not set, all characters are accepted.
    std::function<bool(char32_t)> inputFilter;

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
