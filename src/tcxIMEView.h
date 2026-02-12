#pragma once

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>

class tcxIMEBase;

// NSView implementing NSTextInputClient protocol
// Receives IME input from OS and forwards to tcxIMEBase
@interface tcxIMEView : NSView <NSTextInputClient>

@property (nonatomic, weak) NSView *originalView;
@property (nonatomic, assign) tcxIMEBase *imeInstance;

@property (nonatomic, strong) NSMutableAttributedString *markedTextStorage;
@property (nonatomic, assign) NSRange markedRange;
@property (nonatomic, assign) NSRange selectedRange;

// Flag: whether IME handled the key event
@property (nonatomic, assign) BOOL handledByIME;

- (instancetype)initWithFrame:(NSRect)frameRect imeInstance:(tcxIMEBase*)ime;
- (void)setOriginalView:(NSView *)view;
- (void)setImeInstance:(tcxIMEBase*)ime;

@end

#endif
