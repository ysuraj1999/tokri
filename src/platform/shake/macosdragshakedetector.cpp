#include "platform/shake/macosdragshakedetector.h"

CGEventRef MacOSDragShakeDetector::eventCallback(CGEventTapProxy,
                                                 CGEventType type,
                                                 CGEventRef event,
                                                 void *userInfo)
{
    auto *self = static_cast<MacOSDragShakeDetector *>(userInfo);
    self->handleEvent(type, event);
    return event;
}

MacOSDragShakeDetector::MacOSDragShakeDetector(QObject *parent)
    : QObject(parent)
{
    CGEventMask mask = CGEventMaskBit(kCGEventLeftMouseDragged) |
                       CGEventMaskBit(kCGEventLeftMouseUp);

    eventTap = CGEventTapCreate(kCGSessionEventTap,
                                kCGHeadInsertEventTap,
                                kCGEventTapOptionListenOnly,
                                mask,
                                &MacOSDragShakeDetector::eventCallback,
                                this);
    if (!eventTap)
        return;

    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);
}

MacOSDragShakeDetector::~MacOSDragShakeDetector()
{
    if (runLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
        CFRelease(runLoopSource);
        runLoopSource = nullptr;
    }
    if (eventTap) {
        CFRelease(eventTap);
        eventTap = nullptr;
    }
}

void MacOSDragShakeDetector::handleEvent(CGEventType type, CGEventRef event)
{
    if (type == kCGEventLeftMouseUp) {
        detector.reset();
        haveLastAbsoluteX = false;
        return;
    }

    CGPoint p = CGEventGetLocation(event);

    int dx = 0;
    if (haveLastAbsoluteX)
        dx = static_cast<int>(p.x - lastAbsoluteX);
    lastAbsoluteX = p.x;
    haveLastAbsoluteX = true;

    if (dx == 0)
        return;

    if (detector.feed(dx))
        emit shakeDetected();
}