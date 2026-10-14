#ifndef LINUXDRAGSHAKEDETECTOR_H
#define LINUXDRAGSHAKEDETECTOR_H

#include <QObject>

#include <atomic>
#include <thread>
#include <vector>

#include "shakedetector.h"

class LinuxDragShakeDetector : public QObject
{
    Q_OBJECT

public:
    explicit LinuxDragShakeDetector(QObject *parent = nullptr);
    ~LinuxDragShakeDetector() override;

signals:
    void shakeDetected();

private:
    struct InputDevice {
        int fd = -1;
        bool hasRelX = false;
        bool hasAbsX = false;
        bool hasAbsMtX = false;
        int absXMin = 0;
        int absXMax = 0;
        int absMtXMin = 0;
        int absMtXMax = 0;
        int lastAbs = 0;
        bool hasAbs = false;
    };

    void scanDevices();
    void workerLoop();

    std::vector<InputDevice> devices;
    std::thread worker;
    std::atomic<bool> running{false};
    bool leftPressed = false;

    ShakeDetector detector;
};

#endif // LINUXDRAGSHAKEDETECTOR_H