#include "tcApp.h"

void tcApp::setup() {
#ifdef __APPLE__
    const char* fontPath = "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc";
#elif defined(_WIN32)
    const char* fontPath = "C:/Windows/Fonts/YuGothM.ttc";
#else
    const char* fontPath = "";
#endif

    // TextField - multi-line text input
    textField_ = make_shared<tcxIME::TextField>();
    textField_->setFont(fontPath, 24);
    textField_->setPos(20, 110);
    textField_->setSize(760, 200);
    textField_->enable();
    addChild(textField_);

    // FloatField - float input with range
    floatField_ = make_shared<tcxIME::FloatField>();
    floatField_->setFont(fontPath, 20);
    floatField_->setPos(20, 370);
    floatField_->setSize(200, 30);
    floatField_->setRange(-100.0f, 100.0f);
    floatField_->setValue(3.14f);
    floatField_->onValueChanged = [](float v) {
        logNotice() << "Float value: " << v;
    };
    floatField_->enable();
    addChild(floatField_);

    // IntField - int input with range
    intField_ = make_shared<tcxIME::IntField>();
    intField_->setFont(fontPath, 20);
    intField_->setPos(20, 430);
    intField_->setSize(200, 30);
    intField_->setRange(0, 255);
    intField_->setValue(128);
    intField_->onValueChanged = [](int v) {
        logNotice() << "Int value: " << v;
    };
    intField_->enable();
    addChild(intField_);
}

void tcApp::draw() {
    clear(0.1f);

    // Title
    setColor(0.5f);
    drawBitmapString("tcxIME Example - Type Japanese!", 20, 30);
#ifdef __APPLE__
    drawBitmapString("Click & drag to select, Cmd+C/X to copy/cut, Cmd+V to paste", 20, 50);
#else
    drawBitmapString("Click & drag to select, Ctrl+C/X to copy/cut, Ctrl+V to paste", 20, 50);
#endif

    // Labels
    setColor(0.45f);
    drawBitmapString("TextField (multi-line):", 20, 90);
    drawBitmapString("FloatField [-100, 100]:", 20, 355);
    drawBitmapString("IntField [0, 255]:", 20, 415);

    // Status
    setColor(0.4f);
    string mode = textField_->isJapaneseMode() ? "Japanese" : "English";
    drawBitmapString("Mode: " + mode, 20, getWindowHeight() - 60);
    drawBitmapString("Text: " + textField_->getString(), 20, getWindowHeight() - 40);
    drawBitmapString("Float: " + to_string(floatField_->getValue()) +
                     "  Int: " + to_string(intField_->getValue()),
                     20, getWindowHeight() - 20);
}
