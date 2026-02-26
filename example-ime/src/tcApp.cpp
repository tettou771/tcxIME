#include "tcApp.h"

void tcApp::setup() {
    // Set up TextField as a Node
    textField_ = make_shared<tcxIME::TextField>();
    textField_->setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 24);
    textField_->setPos(20, 100);
    textField_->setSize(760, 400);
    textField_->enable();
    addChild(textField_);
}

void tcApp::draw() {
    clear(0.1f);

    // Title
    setColor(0.5f);
    drawBitmapString("tcxIME Example - Type Japanese!", 20, 30);
    drawBitmapString("Click & drag to select, Cmd+C to copy, Cmd+V to paste", 20, 50);

    // TextField draws itself as a child Node

    // Status
    setColor(0.4f);
    string mode = textField_->isJapaneseMode() ? "Japanese" : "English";
    drawBitmapString("Mode: " + mode, 20, getWindowHeight() - 40);
    drawBitmapString("Text: " + textField_->getString(), 20, getWindowHeight() - 20);
}
