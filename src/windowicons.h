#ifndef WINDOWICONS_H
#define WINDOWICONS_H

class QApplication;
class QWidget;

namespace WindowIcons {

// Applies the platform's window/app icons. All per-OS logic lives in this
// one function.
void apply(QApplication &app, QWidget &window);

} // namespace WindowIcons

#endif // WINDOWICONS_H
