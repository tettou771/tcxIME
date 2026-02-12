// tcxIMEView.mm
// NSTextInputClient implementation for macOS IME support

#import "tcxIMEView.h"
#include <vector>

class tcxIMEBase;

// C++ callback functions (implemented in tcxIME_mac.mm)
extern "C" {
    void tcxIME_insertText(tcxIMEBase* ime, const char32_t* str, size_t len);
    void tcxIME_setMarkedText(tcxIMEBase* ime, const char32_t* str, size_t len, int selLoc, int selLen);
    void tcxIME_unmarkText(tcxIMEBase* ime);
    void tcxIME_getMarkedTextScreenPosition(tcxIMEBase* ime, float* x, float* y);
}

@implementation tcxIMEView

- (instancetype)initWithFrame:(NSRect)frameRect imeInstance:(tcxIMEBase*)ime {
    self = [super initWithFrame:frameRect];
    if (self) {
        _originalView = nil;
        _imeInstance = ime;
        _markedTextStorage = [[NSMutableAttributedString alloc] init];
        _markedRange = NSMakeRange(NSNotFound, 0);
        _selectedRange = NSMakeRange(0, 0);
        _handledByIME = NO;
    }
    return self;
}

- (void)setOriginalView:(NSView *)view {
    _originalView = view;
}

- (void)setImeInstance:(tcxIMEBase *)ime {
    _imeInstance = ime;
}

// Forward key events to IME and original view
- (void)keyDown:(NSEvent *)event {
    _handledByIME = NO;

    // Let IME process the event
    [self interpretKeyEvents:@[event]];

    // Only forward to original view if IME didn't handle it
    if (!_handledByIME && _originalView) {
        [_originalView keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event {
    if (_originalView) {
        [_originalView keyUp:event];
    }
}

- (void)flagsChanged:(NSEvent *)event {
    if (_originalView) {
        [_originalView flagsChanged:event];
    }
}

// Forward mouse events
- (void)mouseDown:(NSEvent *)event {
    if (_originalView) [_originalView mouseDown:event];
}

- (void)mouseUp:(NSEvent *)event {
    if (_originalView) [_originalView mouseUp:event];
}

- (void)mouseMoved:(NSEvent *)event {
    if (_originalView) [_originalView mouseMoved:event];
}

- (void)mouseDragged:(NSEvent *)event {
    if (_originalView) [_originalView mouseDragged:event];
}

- (void)scrollWheel:(NSEvent *)event {
    if (_originalView) [_originalView scrollWheel:event];
}

- (void)rightMouseDown:(NSEvent *)event {
    if (_originalView) [_originalView rightMouseDown:event];
}

- (void)rightMouseUp:(NSEvent *)event {
    if (_originalView) [_originalView rightMouseUp:event];
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

#pragma mark - NSTextInputClient Protocol

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    _handledByIME = YES;

    NSString *insertString;
    if ([string isKindOfClass:[NSAttributedString class]]) {
        insertString = [(NSAttributedString*)string string];
    } else {
        insertString = (NSString*)string;
    }

    // Skip control characters
    if (insertString.length == 1) {
        unichar c = [insertString characterAtIndex:0];
        if (c < 0x20 || c == 0x7F) {
            return;
        }
    }

    // Clear marked text
    _markedRange = NSMakeRange(NSNotFound, 0);
    [_markedTextStorage setAttributedString:[[NSAttributedString alloc] init]];

    // Pass confirmed text to tcxIME
    if (_imeInstance && insertString.length > 0) {
        std::vector<char32_t> u32vec;
        for (NSUInteger i = 0; i < insertString.length; ++i) {
            unichar c = [insertString characterAtIndex:i];
            if (CFStringIsSurrogateHighCharacter(c) && i + 1 < insertString.length) {
                unichar low = [insertString characterAtIndex:++i];
                if (CFStringIsSurrogateLowCharacter(low)) {
                    u32vec.push_back(CFStringGetLongCharacterForSurrogatePair(c, low));
                }
            } else {
                u32vec.push_back((char32_t)c);
            }
        }
        tcxIME_insertText(_imeInstance, u32vec.data(), u32vec.size());
    }
}

- (void)doCommandBySelector:(SEL)selector {
    NSEventModifierFlags modifiers = [NSEvent modifierFlags];
    BOOL hasModifier = (modifiers & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0;

    if (!hasModifier &&
        (selector == @selector(insertNewline:) ||
         selector == @selector(insertLineBreak:))) {
        _handledByIME = YES;
        if (_imeInstance) {
            char32_t newline = U'\n';
            tcxIME_insertText(_imeInstance, &newline, 1);
        }
        return;
    }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    _handledByIME = YES;

    NSString *markedString;
    if ([string isKindOfClass:[NSAttributedString class]]) {
        [_markedTextStorage setAttributedString:(NSAttributedString*)string];
        markedString = [(NSAttributedString*)string string];
    } else {
        [_markedTextStorage setAttributedString:[[NSAttributedString alloc] initWithString:(NSString*)string]];
        markedString = (NSString*)string;
    }

    if (markedString.length > 0) {
        _markedRange = NSMakeRange(0, markedString.length);
        _selectedRange = selectedRange;
    } else {
        _markedRange = NSMakeRange(NSNotFound, 0);
        _selectedRange = NSMakeRange(0, 0);
    }

    if (_imeInstance) {
        std::vector<char32_t> u32vec;
        for (NSUInteger i = 0; i < markedString.length; ++i) {
            unichar c = [markedString characterAtIndex:i];
            if (CFStringIsSurrogateHighCharacter(c) && i + 1 < markedString.length) {
                unichar low = [markedString characterAtIndex:++i];
                if (CFStringIsSurrogateLowCharacter(low)) {
                    u32vec.push_back(CFStringGetLongCharacterForSurrogatePair(c, low));
                }
            } else {
                u32vec.push_back((char32_t)c);
            }
        }
        tcxIME_setMarkedText(_imeInstance, u32vec.data(), u32vec.size(),
                            (int)selectedRange.location, (int)selectedRange.length);
    }
}

- (void)unmarkText {
    _markedRange = NSMakeRange(NSNotFound, 0);
    [_markedTextStorage setAttributedString:[[NSAttributedString alloc] init]];

    if (_imeInstance) {
        tcxIME_unmarkText(_imeInstance);
    }
}

- (NSRange)selectedRange {
    return _selectedRange;
}

- (NSRange)markedRange {
    return _markedRange;
}

- (BOOL)hasMarkedText {
    return _markedRange.location != NSNotFound && _markedRange.length > 0;
}

- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
    if (_markedRange.location != NSNotFound && _markedTextStorage.length > 0) {
        if (range.location < _markedTextStorage.length) {
            NSRange adjustedRange = NSMakeRange(range.location, MIN(range.length, _markedTextStorage.length - range.location));
            if (actualRange) {
                *actualRange = adjustedRange;
            }
            return [_markedTextStorage attributedSubstringFromRange:adjustedRange];
        }
    }
    return nil;
}

- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
    return @[NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName];
}

// Return screen rect for IME candidate window positioning
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
    if (!_imeInstance) {
        return NSMakeRect(0, 0, 0, 0);
    }

    float posX = 0, posY = 0;
    tcxIME_getMarkedTextScreenPosition(_imeInstance, &posX, &posY);

    NSWindow *window = self.window;
    if (!window) {
        return NSMakeRect(0, 0, 0, 0);
    }

    NSRect windowFrame = [window frame];
    NSRect contentRect = [window contentRectForFrameRect:windowFrame];

    // sokol_app uses top-left origin, macOS uses bottom-left
    CGFloat windowX = posX;
    CGFloat windowY = contentRect.size.height - posY;

    NSPoint screenPoint = [window convertPointToScreen:NSMakePoint(windowX, windowY)];

    return NSMakeRect(screenPoint.x, screenPoint.y - 20, 0, 20);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    return NSNotFound;
}

@end
