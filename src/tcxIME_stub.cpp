// Stub implementations for non-macOS platforms (Linux, Windows, Web)
// These provide empty implementations so the addon compiles everywhere

#if !defined(__APPLE__)

#include "tcxIME.h"

void tcxIMEBase::startIMEObserver() {}
void tcxIMEBase::stopIMEObserver() {}
void tcxIMEBase::syncWithSystemIME() {}

#endif
