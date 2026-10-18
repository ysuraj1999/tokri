#include "platform/shake/windowsdragshakedetector.h"

WindowsDragShakeDetector *WindowsDragShakeDetector::instance = nullptr;
HHOOK WindowsDragShakeDetector::mouseHook = nullptr;

WindowsDragShakeDetector::WindowsDragShakeDetector(QObject *parent)
    : QObject(parent)
{
    instance = this;
    running = true;
    worker = std::thread(&WindowsDragShakeDetector::workerLoop, this);
}

WindowsDragShakeDetector::~WindowsDragShakeDetector()
{
    running = false;
    const DWORD tid = threadId.load();
    if (tid)
        PostThreadMessage(tid, WM_QUIT, 0, 0);
    if (worker.joinable())
        worker.join();
    if (mouseHook) {
        UnhookWindowsHookEx(mouseHook);
        mouseHook = nullptr;
    }
    instance = nullptr;
}

void WindowsDragShakeDetector::workerLoop()
{
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseProc, nullptr, 0);
    if (!mouseHook) {
        running = false;
        return;
    }

    threadId = GetCurrentThreadId();

    // Message loop (required for WH_MOUSE_LL)
    MSG msg;
    while (running && GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(mouseHook);
    mouseHook = nullptr;
}

LRESULT CALLBACK WindowsDragShakeDetector::mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && instance) {
        MSLLHOOKSTRUCT *hookStruct = (MSLLHOOKSTRUCT*)lParam;

        switch (wParam) {
        case WM_MOUSEMOVE:
            if (instance->leftPressed)
                instance->handleMove(hookStruct->pt.x);
            break;
        case WM_LBUTTONDOWN:
            instance->handleButton(true);
            break;
        case WM_LBUTTONUP:
            instance->handleButton(false);
            break;
        default:
            break;
        }
    }
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

void WindowsDragShakeDetector::handleMove(int absoluteX)
{
    int dx = 0;
    if (haveLastAbsoluteX)
        dx = absoluteX - lastAbsoluteX;
    lastAbsoluteX = absoluteX;
    haveLastAbsoluteX = true;

    if (dx == 0)
        return;

    if (detector.feed(dx))
        emit shakeDetected();
}

void WindowsDragShakeDetector::handleButton(bool pressed)
{
    leftPressed = pressed;
    if (!pressed) {
        detector.reset();
        haveLastAbsoluteX = false;
    }
}
