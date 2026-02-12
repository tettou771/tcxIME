// macOS-specific IME implementation

#ifdef __APPLE__

#include "tcxIME.h"
#include "tcxIMEView.h"

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <sokol/sokol_app.h>

using namespace std;
// NOTE: Do NOT 'using namespace tc;' here - macOS Rect conflicts with trussc::Rect

// Static member definitions
void* tcxIMEBase::sharedIMEView_ = nullptr;
void* tcxIMEBase::sharedOriginalContentView_ = nullptr;
tcxIMEBase* tcxIMEBase::activeIMEInstance_ = nullptr;
int tcxIMEBase::imeViewRefCount_ = 0;

void tcxIMEBase::startIMEObserver() {
    CFNotificationCenterAddObserver(
        CFNotificationCenterGetDistributedCenter(),
        this,
        onInputSourceChanged,
        kTISNotifySelectedKeyboardInputSourceChanged,
        NULL,
        CFNotificationSuspensionBehaviorDeliverImmediately
    );

    syncWithSystemIME();
}

void tcxIMEBase::stopIMEObserver() {
    CFNotificationCenterRemoveObserver(
        CFNotificationCenterGetDistributedCenter(),
        this,
        kTISNotifySelectedKeyboardInputSourceChanged,
        NULL
    );
}

void tcxIMEBase::onInputSourceChanged(CFNotificationCenterRef center,
                                      void* observer,
                                      CFNotificationName name,
                                      const void* object,
                                      CFDictionaryRef userInfo) {
    tcxIMEBase* ime = static_cast<tcxIMEBase*>(observer);
    if (ime) {
        ime->syncWithSystemIME();
    }
}

void tcxIMEBase::syncWithSystemIME() {
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    if (source) {
        CFStringRef sourceID = (CFStringRef)TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
        if (sourceID) {
            bool isJapanese = (CFStringFind(sourceID, CFSTR("Japanese"), 0).location != kCFNotFound) ||
                              (CFStringFind(sourceID, CFSTR("Hiragana"), 0).location != kCFNotFound);

            if (isJapanese) {
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
        CFRelease(source);
    }
}

void tcxIMEBase::setupIMEInputView() {
    imeViewRefCount_++;

    if (sharedIMEView_ != nullptr) {
        becomeActiveIME();
        return;
    }

    // Get NSWindow from sokol_app
    NSWindow* nsWindow = (__bridge NSWindow*)sapp_macos_get_window();
    if (!nsWindow) return;

    NSView* contentView = [nsWindow contentView];
    if (!contentView) return;

    sharedOriginalContentView_ = (__bridge void*)contentView;

    // Create custom IME view
    tcxIMEView* customView = [[tcxIMEView alloc] initWithFrame:[contentView frame] imeInstance:this];
    [customView setOriginalView:contentView];
    [customView setAutoresizingMask:[contentView autoresizingMask]];

    sharedIMEView_ = (__bridge_retained void*)customView;
    activeIMEInstance_ = this;

    // Add as subview and make first responder
    [contentView addSubview:customView];
    [nsWindow makeFirstResponder:customView];
}

void tcxIMEBase::removeIMEInputView() {
    imeViewRefCount_--;

    if (imeViewRefCount_ > 0) {
        if (activeIMEInstance_ == this) {
            activeIMEInstance_ = nullptr;
        }
        return;
    }

    if (sharedIMEView_ == nullptr) return;

    tcxIMEView* customView = (__bridge_transfer tcxIMEView*)sharedIMEView_;
    sharedIMEView_ = nullptr;
    activeIMEInstance_ = nullptr;

    [customView removeFromSuperview];

    if (sharedOriginalContentView_ != nullptr) {
        NSView* origView = (__bridge NSView*)sharedOriginalContentView_;
        NSWindow* nsWindow = [origView window];
        if (nsWindow) {
            [nsWindow makeFirstResponder:origView];
        }
        sharedOriginalContentView_ = nullptr;
    }
}

void tcxIMEBase::becomeActiveIME() {
    if (sharedIMEView_ == nullptr) return;

    tcxIMEView* customView = (__bridge tcxIMEView*)sharedIMEView_;
    [customView setImeInstance:this];
    activeIMEInstance_ = this;
}

// C-style callback functions for tcxIMEView
extern "C" {

void tcxIME_insertText(tcxIMEBase* ime, const char32_t* str, size_t len) {
    if (ime && str) {
        u32string u32str(str, len);
        ime->insertText(u32str);
    }
}

void tcxIME_setMarkedText(tcxIMEBase* ime, const char32_t* str, size_t len, int selLoc, int selLen) {
    if (ime) {
        u32string u32str(str, len);
        ime->setMarkedTextFromOS(u32str, selLoc, selLen);
    }
}

void tcxIME_unmarkText(tcxIMEBase* ime) {
    if (ime) {
        ime->unmarkText();
    }
}

void tcxIME_getMarkedTextScreenPosition(tcxIMEBase* ime, float* x, float* y) {
    if (ime && x && y) {
        tc::Vec2 pos = ime->getMarkedTextScreenPosition();
        *x = pos.x;
        *y = pos.y;
    }
}

} // extern "C"

#endif
