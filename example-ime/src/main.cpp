#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(800, 600);
    settings.title = "tcxIME Example";

    return tc::runApp<tcApp>(settings);
}
