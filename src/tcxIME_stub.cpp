// Stub implementations for non-macOS platforms (Linux, Windows, Web)
// These provide empty implementations so the addon compiles everywhere

#if !defined(__APPLE__) && !defined(_WIN32)

#include "tcxIME.h"

tcxIMEBase* tcxIMEBase::activeIMEInstance_ = nullptr;

void tcxIMEBase::startIMEObserver() {}
void tcxIMEBase::stopIMEObserver() {}
void tcxIMEBase::syncWithSystemIME() {}

#endif
