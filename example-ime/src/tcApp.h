#pragma once
#include <TrussC.h>
#include <tcxIME.h>

using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void draw() override;

private:
    tcxIME ime_;
};
