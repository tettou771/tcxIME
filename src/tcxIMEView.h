#pragma once

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>

namespace tcx { namespace ime { class IMEBase; } }

// NSView implementing NSTextInputClient protocol
// Receives IME input from OS and forwards to tcx::ime::IMEBase
@interface tcxIMEView : NSView <NSTextInputClient>

@property (nonatomic, weak) NSView *originalView;
@property (nonatomic, assign) tcx::ime::IMEBase *imeInstance;

@property (nonatomic, strong) NSMutableAttributedString *markedTextStorage;
@property (nonatomic, assign) NSRange markedRange;
@property (nonatomic, assign) NSRange selectedRange;

// Flag: whether IME handled the key event
@property (nonatomic, assign) BOOL handledByIME;

- (instancetype)initWithFrame:(NSRect)frameRect imeInstance:(tcx::ime::IMEBase*)ime;
- (void)setOriginalView:(NSView *)view;
- (void)setImeInstance:(tcx::ime::IMEBase*)ime;

@end

#endif
