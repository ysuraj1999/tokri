#ifndef MACOSDRAGSHAKEDETECTOR_H
#define MACOSDRAGSHAKEDETECTOR_H

#include <QObject>
#include <CoreGraphics/CoreGraphics.h>

#include "platform/shake/shakedetector.h"

class MacOSDragShakeDetector : public QObject
{
    Q_OBJECT

public:
    explicit MacOSDragShakeDetector(QObject *parent = nullptr);
    ~MacOSDragShakeDetector() override;

signals:
    void shakeDetected();

private:
    static CGEventRef eventCallback(CGEventTapProxy, CGEventType, CGEventRef, void *userInfo);
    void handleEvent(CGEventType type, CGEventRef event);

    CFMachPortRef eventTap = nullptr;
    CFRunLoopSourceRef runLoopSource = nullptr;

    bool haveLastAbsoluteX = false;
    double lastAbsoluteX = 0.0;

    ShakeDetector detector;
};

#endif // MACOSDRAGSHAKEDETECTOR_H