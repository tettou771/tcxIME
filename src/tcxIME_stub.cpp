// Stub implementations for non-macOS platforms (Linux, Windows, Web)
// These provide empty implementations so the addon compiles everywhere

#if !defined(__APPLE__) && !defined(_WIN32)

#include "tcxIME.h"

namespace tcx { namespace ime {

IMEBase* IMEBase::activeIMEInstance_ = nullptr;

void IMEBase::startIMEObserver() {}
void IMEBase::stopIMEObserver() {}
void IMEBase::syncWithSystemIME() {}
void IMEBase::setJapaneseMode(bool) {}

} } // namespace tcx::ime

#endif
