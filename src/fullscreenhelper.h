#ifndef FULLSCREENHELPER_H
#define FULLSCREENHELPER_H

class QWidget;

namespace FullscreenHelper {

// Prompts for system permissions on startup if required (e.g. macOS Accessibility).
void initPlatformPermissions();

// Returns true if the foreground application is currently in fullscreen or gaming mode.
bool isForegroundFullscreenOrGaming(QWidget *selfWindow = nullptr);

} // namespace FullscreenHelper

#endif // FULLSCREENHELPER_H
