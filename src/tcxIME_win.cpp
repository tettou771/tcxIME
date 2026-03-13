// Windows-specific IME implementation using IMM32 API

#ifdef _WIN32

#include "tcxIME.h"
#include <sokol/sokol_app.h>
#include <string>

using namespace std;

// Static member definitions
tcxIMEBase* tcxIMEBase::activeIMEInstance_ = nullptr;
WNDPROC tcxIMEBase::originalWndProc_ = nullptr;
int tcxIMEBase::imeViewRefCount_ = 0;
bool tcxIMEBase::suppressNextChar_ = false;

// ---------------------------------------------------------------------------
// UTF-16 (wchar_t) to UTF-32 conversion with surrogate pair support
// ---------------------------------------------------------------------------
static u32string WideToU32(const wchar_t* wstr, int len) {
    u32string result;
    for (int i = 0; i < len; i++) {
        wchar_t w = wstr[i];
        if (w >= 0xD800 && w <= 0xDBFF && i + 1 < len) {
            // High surrogate
            wchar_t w2 = wstr[i + 1];
            if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                // Low surrogate
                char32_t cp = 0x10000 + ((char32_t)(w - 0xD800) << 10) + (w2 - 0xDC00);
                result += cp;
                i++;
                continue;
            }
        }
        result += (char32_t)w;
    }
    return result;
}

// ---------------------------------------------------------------------------
// WndProc subclass for intercepting IME messages
// ---------------------------------------------------------------------------
LRESULT CALLBACK tcxIMEBase::IMEWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    tcxIMEBase* ime = activeIMEInstance_;

    switch (msg) {

    case WM_IME_STARTCOMPOSITION:
        // Suppress default composition window
        return 0;

    case WM_IME_COMPOSITION: {
        if (!ime) break;

        HIMC hIMC = ImmGetContext(hWnd);
        if (!hIMC) break;

        // Handle confirmed (result) string
        if (lParam & GCS_RESULTSTR) {
            int bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
            if (bytes > 0) {
                int wlen = bytes / sizeof(wchar_t);
                wchar_t* buf = new wchar_t[wlen + 1];
                ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf, bytes);
                buf[wlen] = 0;

                u32string u32str = WideToU32(buf, wlen);
                delete[] buf;

                ime->insertText(u32str);
                suppressNextChar_ = true;
            }
        }

        // Handle composing string
        if (lParam & GCS_COMPSTR) {
            int bytes = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, NULL, 0);
            int wlen = bytes / sizeof(wchar_t);

            if (wlen > 0) {
                wchar_t* buf = new wchar_t[wlen + 1];
                ImmGetCompositionStringW(hIMC, GCS_COMPSTR, buf, bytes);
                buf[wlen] = 0;

                u32string u32str = WideToU32(buf, wlen);
                delete[] buf;

                // Get attribute info to find target-converted range
                int selLoc = 0;
                int selLen = 0;
                int attrBytes = ImmGetCompositionStringW(hIMC, GCS_COMPATTR, NULL, 0);
                if (attrBytes > 0) {
                    BYTE* attr = new BYTE[attrBytes];
                    ImmGetCompositionStringW(hIMC, GCS_COMPATTR, attr, attrBytes);

                    // Find ATTR_TARGET_CONVERTED or ATTR_TARGET_NOTCONVERTED range
                    int targetStart = -1;
                    int targetEnd = -1;
                    for (int i = 0; i < attrBytes; i++) {
                        if (attr[i] == ATTR_TARGET_CONVERTED || attr[i] == ATTR_TARGET_NOTCONVERTED) {
                            if (targetStart < 0) targetStart = i;
                            targetEnd = i + 1;
                        }
                    }
                    delete[] attr;

                    if (targetStart >= 0) {
                        // Convert byte attribute indices to u32string indices
                        // For BMP characters, attribute index == wchar_t index
                        // Need to account for surrogate pairs when mapping to u32 positions
                        selLoc = 0;
                        selLen = 0;
                        int u32pos = 0;
                        // Re-read the composition string for mapping
                        wchar_t* mapBuf = new wchar_t[wlen + 1];
                        ImmGetCompositionStringW(hIMC, GCS_COMPSTR, mapBuf, bytes);
                        mapBuf[wlen] = 0;

                        int wi = 0;
                        while (wi < wlen) {
                            bool isSurrogate = (mapBuf[wi] >= 0xD800 && mapBuf[wi] <= 0xDBFF && wi + 1 < wlen);
                            if (wi == targetStart) selLoc = u32pos;
                            if (wi >= targetStart && wi < targetEnd) {
                                selLen++;
                                if (isSurrogate) wi++; // skip low surrogate in wchar index
                            } else {
                                if (isSurrogate) wi++;
                            }
                            u32pos++;
                            wi++;
                        }
                        delete[] mapBuf;
                    }
                }

                ime->setMarkedTextFromOS(u32str, selLoc, selLen);
            } else {
                // Empty composition string — clear marked text
                ime->setMarkedTextFromOS(U"", 0, 0);
            }
        }

        ImmReleaseContext(hWnd, hIMC);
        return 0;
    }

    case WM_IME_ENDCOMPOSITION:
        if (ime) {
            ime->unmarkText();
        }
        return 0;

    case WM_IME_SETCONTEXT:
        // Remove composition window flag so OS doesn't draw its own
        lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    case WM_INPUTLANGCHANGE:
        if (ime) {
            ime->syncWithSystemIME();
        }
        break;

    case WM_CHAR:
        if (ime && ime->isEnabled()) {
            // Suppress WM_CHAR that follows GCS_RESULTSTR
            if (suppressNextChar_) {
                suppressNextChar_ = false;
                return 0;
            }

            wchar_t wc = (wchar_t)wParam;

            // Let control characters through to sokol (backspace, enter, etc.)
            if (wc < 0x20) {
                break;
            }

            // Route printable characters through insertText
            // (This handles direct typing in English mode)
            static wchar_t pendingHighSurrogate = 0;
            u32string u32str;
            // Handle surrogate pairs for WM_CHAR
            if (wc >= 0xD800 && wc <= 0xDBFF) {
                // High surrogate — wait for WM_CHAR with low surrogate
                pendingHighSurrogate = wc;
                return 0;
            } else if (wc >= 0xDC00 && wc <= 0xDFFF) {
                if (pendingHighSurrogate) {
                    char32_t cp = 0x10000 + ((char32_t)(pendingHighSurrogate - 0xD800) << 10) + (wc - 0xDC00);
                    u32str += cp;
                    pendingHighSurrogate = 0;
                }
            } else {
                pendingHighSurrogate = 0;
                u32str += (char32_t)wc;
            }

            if (!u32str.empty()) {
                ime->insertText(u32str);
            }
            return 0;
        }
        break;
    }

    return CallWindowProcW(originalWndProc_, hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// IME Observer
// ---------------------------------------------------------------------------
void tcxIMEBase::startIMEObserver() {
    syncWithSystemIME();
}

void tcxIMEBase::stopIMEObserver() {
    // Nothing to do — subclassing is managed by setupIMEInputView/removeIMEInputView
}

void tcxIMEBase::setJapaneseMode(bool japanese) {
    HWND hWnd = (HWND)sapp_win32_get_hwnd();
    if (!hWnd) return;

    HIMC hIMC = ImmGetContext(hWnd);
    if (!hIMC) return;

    if (japanese) {
        ImmSetOpenStatus(hIMC, TRUE);
    } else {
        ImmSetOpenStatus(hIMC, FALSE);
    }
    ImmReleaseContext(hWnd, hIMC);

    syncWithSystemIME();
}

void tcxIMEBase::syncWithSystemIME() {
    HWND hWnd = (HWND)sapp_win32_get_hwnd();
    if (!hWnd) return;

    HKL hkl = GetKeyboardLayout(0);
    // Check if the current keyboard layout is Japanese (LANGID 0x0411)
    LANGID langId = LOWORD((DWORD_PTR)hkl);
    bool isJapanese = (PRIMARYLANGID(langId) == LANG_JAPANESE);

    if (isJapanese) {
        // Also check if IME is open (converting mode vs direct input)
        HIMC hIMC = ImmGetContext(hWnd);
        if (hIMC) {
            bool imeOpen = ImmGetOpenStatus(hIMC) != 0;
            ImmReleaseContext(hWnd, hIMC);

            if (imeOpen) {
                if (state_ == Eisu) {
                    state_ = Kana;
                }
            } else {
                if (state_ == Composing) {
                    unmarkText();
                }
                state_ = Eisu;
            }
        }
    } else {
        if (state_ == Composing) {
            unmarkText();
        }
        state_ = Eisu;
    }
}

// ---------------------------------------------------------------------------
// Window subclassing
// ---------------------------------------------------------------------------
void tcxIMEBase::setupIMEInputView() {
    imeViewRefCount_++;

    if (originalWndProc_ != nullptr) {
        // Already subclassed — just become active
        becomeActiveIME();
        return;
    }

    HWND hWnd = (HWND)sapp_win32_get_hwnd();
    if (!hWnd) return;

    // Subclass sokol's WndProc
    originalWndProc_ = (WNDPROC)SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)IMEWndProc);
    activeIMEInstance_ = this;
}

void tcxIMEBase::removeIMEInputView() {
    imeViewRefCount_--;

    if (activeIMEInstance_ == this) {
        activeIMEInstance_ = nullptr;
    }

    if (imeViewRefCount_ > 0) {
        return;
    }

    // Restore original WndProc
    if (originalWndProc_ != nullptr) {
        HWND hWnd = (HWND)sapp_win32_get_hwnd();
        if (hWnd) {
            SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc_);
        }
        originalWndProc_ = nullptr;
    }
}

void tcxIMEBase::becomeActiveIME() {
    activeIMEInstance_ = this;
}

#endif // _WIN32
