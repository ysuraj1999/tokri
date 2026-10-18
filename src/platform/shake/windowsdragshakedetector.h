#ifndef WINDOWSDRAGSHAKEDETECTOR_H
#define WINDOWSDRAGSHAKEDETECTOR_H

#include <QObject>

#include <atomic>
#include <thread>

#include <Windows.h>

#include "platform/shake/shakedetector.h"

class WindowsDragShakeDetector : public QObject
{
    Q_OBJECT

public:
    explicit WindowsDragShakeDetector(QObject *parent = nullptr);
    ~WindowsDragShakeDetector() override;

signals:
    void shakeDetected();

private:
    static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    void workerLoop();
    void handleMove(int absoluteX);
    void handleButton(bool pressed);

    static WindowsDragShakeDetector *instance;
    static HHOOK mouseHook;

    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<DWORD> threadId{0};

    bool leftPressed = false;
    bool haveLastAbsoluteX = false;
    int lastAbsoluteX = 0;

    ShakeDetector detector;
};

#endif // WINDOWSDRAGSHAKEDETECTOR_H
