#ifndef LINUXDRAGSHAKEDETECTOR_H
#define LINUXDRAGSHAKEDETECTOR_H

#include <QObject>

#include <atomic>
#include <thread>
#include <vector>

#include "platform/shake/shakedetector.h"

class LinuxDragShakeDetector : public QObject
{
    Q_OBJECT

public:
    explicit LinuxDragShakeDetector(QObject *parent = nullptr);
    ~LinuxDragShakeDetector() override;

signals:
    void shakeDetected();

private:
    enum class AxisMode { None, Rel, AbsX, AbsMt };

    void scanDevices();
    void workerLoop();

    std::vector<int> fds;
    std::thread worker;
    std::atomic<bool> running{false};
    bool leftPressed = false;

    AxisMode mode = AxisMode::None;
    int lastAbs = 0;
    bool hasAbs = false;

    ShakeDetector detector;
};

#endif // LINUXDRAGSHAKEDETECTOR_H