#include "linuxdragshakedetector.h"

#include "loghelpers.h"

#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <linux/input.h>

#include <QDebug>

namespace {
constexpr int kPollTimeoutMs = 100;
// Maximum realistic single-event movement in normalized pixels (filters teleportation)
constexpr int kMaxNormalizedDelta = 500;
constexpr double kVirtualScreenWidth = 1920.0;

bool has_bit(unsigned long *bits, int nr)
{
    return bits[nr / (8 * sizeof(unsigned long))] &
           (1UL << (nr % (8 * sizeof(unsigned long))));
}
}

LinuxDragShakeDetector::LinuxDragShakeDetector(QObject *parent)
    : QObject(parent)
{
    scanDevices();
    if (!devices.empty()) {
        running = true;
        worker = std::thread(&LinuxDragShakeDetector::workerLoop, this);
    }
}

LinuxDragShakeDetector::~LinuxDragShakeDetector()
{
    running = false;
    if (worker.joinable())
        worker.join();
    for (const auto &dev : devices) {
        if (dev.fd >= 0)
            ::close(dev.fd);
    }
}

void LinuxDragShakeDetector::scanDevices()
{
    Logger &log = Logger::instance();
    log.push("scanDevices");

    DIR *dir = opendir("/dev/input");
    if (!dir) {
        qWarning() << "Failed to open /dev/input. Shake detection disabled.";
        log.pop();
        return;
    }

    log.log() << "Scanning /dev/input for pointer devices:";
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

        int probe = ::open(path, O_RDONLY);
        if (probe < 0)
            continue;

        unsigned long evbits[EV_MAX / (8 * sizeof(unsigned long)) + 1] = {};
        if (ioctl(probe, EVIOCGBIT(0, sizeof(evbits)), evbits) < 0) {
            ::close(probe);
            continue;
        }

        bool relX = false;
        if (has_bit(evbits, EV_REL)) {
            unsigned long relbits[REL_MAX / (8 * sizeof(unsigned long)) + 1] = {};
            if (ioctl(probe, EVIOCGBIT(EV_REL, sizeof(relbits)), relbits) >= 0)
                relX = has_bit(relbits, REL_X);
        }

        bool absX = false;
        bool absMtX = false;
        int absXMin = 0, absXMax = 0;
        int absMtXMin = 0, absMtXMax = 0;

        if (has_bit(evbits, EV_ABS)) {
            unsigned long absbits[ABS_MAX / (8 * sizeof(unsigned long)) + 1] = {};
            if (ioctl(probe, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) >= 0) {
                absX = has_bit(absbits, ABS_X);
                absMtX = has_bit(absbits, ABS_MT_POSITION_X);
            }

            if (absX) {
                struct input_absinfo absInfo{};
                if (ioctl(probe, EVIOCGABS(ABS_X), &absInfo) >= 0) {
                    absXMin = absInfo.minimum;
                    absXMax = absInfo.maximum;
                }
            }

            if (absMtX) {
                struct input_absinfo mtInfo{};
                if (ioctl(probe, EVIOCGABS(ABS_MT_POSITION_X), &mtInfo) >= 0) {
                    absMtXMin = mtInfo.minimum;
                    absMtXMax = mtInfo.maximum;
                }
            }
        }

        if (relX || absX || absMtX) {
            log.log() << path << (relX ? " [rel]" : "")
                      << (absX ? QString(" [abs: %1..%2]").arg(absXMin).arg(absXMax) : QString())
                      << (absMtX ? QString(" [absMt: %1..%2]").arg(absMtXMin).arg(absMtXMax) : QString());

            InputDevice dev;
            dev.fd = probe;
            dev.hasRelX = relX;
            dev.hasAbsX = absX;
            dev.hasAbsMtX = absMtX;
            dev.absXMin = absXMin;
            dev.absXMax = absXMax;
            dev.absMtXMin = absMtXMin;
            dev.absMtXMax = absMtXMax;
            devices.push_back(dev);
        } else {
            ::close(probe);
        }
    }

    closedir(dir);

    if (devices.empty()) {
        qWarning() << "No accessible input device with REL_X, ABS_X or ABS_MT_POSITION_X found in /dev/input."
                   << "Shake detection requires read access to /dev/input (add user to 'input' group: sudo usermod -aG input $USER).";
    }

    log.pop();
}

void LinuxDragShakeDetector::workerLoop()
{
    Logger &log = Logger::instance();
    log.push("workerLoop");

    std::vector<struct pollfd> pfds(devices.size());
    for (size_t i = 0; i < devices.size(); ++i) {
        pfds[i].fd = devices[i].fd;
        pfds[i].events = POLLIN;
    }

    struct input_event buffer[64];

    while (running) {
        int n = poll(pfds.data(), pfds.size(), kPollTimeoutMs);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            log.log() << "poll failed:" << strerror(errno);
            break;
        }
        if (n == 0)
            continue;

        for (size_t i = 0; i < pfds.size(); ++i) {
            if (!(pfds[i].revents & POLLIN))
                continue;

            InputDevice &dev = devices[i];
            ssize_t bytes = ::read(dev.fd, buffer, sizeof(buffer));
            if (bytes <= 0) {
                log.log() << "failed to read input events from fd" << dev.fd;
                continue;
            }

            size_t count = (size_t)bytes / sizeof(struct input_event);
            for (size_t j = 0; j < count; ++j) {
                const struct input_event &ev = buffer[j];

                if (ev.type == EV_KEY && ev.code == BTN_LEFT) {
                    if (ev.value == 1) {
                        leftPressed = true;
                        for (auto &d : devices)
                            d.hasAbs = false;
                    } else if (ev.value == 0) {
                        leftPressed = false;
                        for (auto &d : devices)
                            d.hasAbs = false;
                        detector.reset();
                    }
                    continue;
                }

                int dx = 0;
                if (ev.type == EV_REL && ev.code == REL_X) {
                    dx = ev.value;
                } else if (ev.type == EV_ABS && (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X)) {
                    const int minV = (ev.code == ABS_MT_POSITION_X) ? dev.absMtXMin : dev.absXMin;
                    const int maxV = (ev.code == ABS_MT_POSITION_X) ? dev.absMtXMax : dev.absXMax;
                    const int range = maxV - minV;

                    if (!dev.hasAbs) {
                        dev.lastAbs = ev.value;
                        dev.hasAbs = true;
                        continue;
                    }

                    const int rawDx = ev.value - dev.lastAbs;
                    dev.lastAbs = ev.value;

                    if (range > 0) {
                        dx = static_cast<int>(std::round(static_cast<double>(rawDx) * kVirtualScreenWidth / range));
                    } else {
                        dx = rawDx;
                    }

                    if (std::abs(dx) > kMaxNormalizedDelta) {
                        detector.reset();
                        continue;
                    }
                } else {
                    continue;
                }

                if (leftPressed && detector.feed(dx)) {
                    log.log() << "Shake detected!";
                    emit shakeDetected();
                }
            }
        }
    }

    log.pop();
}
