#include "tcApp.h"

void tcApp::setup() {
    // Load Japanese font (Hiragino on macOS)
    ime_.setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 24);
    ime_.enable();
}

void tcApp::draw() {
    clear(0.1f);

    // Title
    setColor(0.5f);
    drawBitmapString("tcxIME Example - Type Japanese!", 20, 30);
    drawBitmapString("Press keys to type. IME conversion supported.", 20, 50);

    // IME text
    setColor(1.0f);
    ime_.draw(20, 100);

    // Status
    setColor(0.4f);
    string mode = ime_.isJapaneseMode() ? "Japanese" : "English";
    drawBitmapString("Mode: " + mode, 20, getWindowHeight() - 40);
    drawBitmapString("Text: " + ime_.getString(), 20, getWindowHeight() - 20);
}
