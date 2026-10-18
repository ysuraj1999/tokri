#include "platform/shake/linuxdragshakedetector.h"

#include "common/loghelpers.h"

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
constexpr int kMaxAbsDelta = 50;

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
    running = true;
    worker = std::thread(&LinuxDragShakeDetector::workerLoop, this);
}

LinuxDragShakeDetector::~LinuxDragShakeDetector()
{
    running = false;
    if (worker.joinable())
        worker.join();
    for (int fd : fds) {
        if (fd >= 0)
            ::close(fd);
    }
}

void LinuxDragShakeDetector::scanDevices()
{
    Logger &log = Logger::instance();
    log.push("scanDevices");

    DIR *dir = opendir("/dev/input");
    if (!dir) {
        qCritical() << "Failed to open /dev/input";
        exit(1);
    }

    log.log() << "REL devices advertising REL_X:";
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
        if (has_bit(evbits, EV_ABS)) {
            unsigned long absbits[ABS_MAX / (8 * sizeof(unsigned long)) + 1] = {};
            if (ioctl(probe, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) >= 0) {
                absX = has_bit(absbits, ABS_X);
                absMtX = has_bit(absbits, ABS_MT_POSITION_X);
            }
        }

        if (relX || absX || absMtX) {
            log.log() << path << (relX ? "rel" : "")
                      << (absMtX ? "mt" : (absX ? "abs" : ""));
            fds.push_back(probe);
        } else {
            ::close(probe);
        }
    }

    closedir(dir);

    if (fds.empty()) {
        qCritical() << "No input device with REL_X, ABS_X or ABS_MT_POSITION_X found";
        exit(1);
    }

    log.pop();
}

void LinuxDragShakeDetector::workerLoop()
{
    Logger &log = Logger::instance();
    log.push("workerLoop");

    std::vector<struct pollfd> pfds(fds.size());
    for (size_t i = 0; i < fds.size(); ++i) {
        pfds[i].fd = fds[i];
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

            ssize_t bytes = ::read(pfds[i].fd, buffer, sizeof(buffer));
            if (bytes <= 0) {
                log.log() << "failed to read input events from fd" << pfds[i].fd;
                continue;
            }

            size_t count = (size_t)bytes / sizeof(struct input_event);
            for (size_t j = 0; j < count; ++j) {
                const struct input_event &ev = buffer[j];

                if (ev.type == EV_KEY && ev.code == BTN_LEFT) {
                    if (ev.value == 1) {
                        leftPressed = true;
                        hasAbs = false;
                    } else if (ev.value == 0) {
                        leftPressed = false;
                        hasAbs = false;
                        detector.reset();
                    }
                    continue;
                }

                AxisMode evMode = AxisMode::None;
                if (ev.type == EV_REL && ev.code == REL_X)
                    evMode = AxisMode::Rel;
                else if (ev.type == EV_ABS && ev.code == ABS_MT_POSITION_X)
                    evMode = AxisMode::AbsMt;
                else if (ev.type == EV_ABS && ev.code == ABS_X &&
                         mode != AxisMode::AbsMt)
                    evMode = AxisMode::AbsX;

                if (evMode == AxisMode::None)
                    continue;

                if (evMode != mode) {
                    mode = evMode;
                    hasAbs = false;
                    detector.reset();
                }

                int dx = ev.value;
                if (mode == AxisMode::AbsX || mode == AxisMode::AbsMt) {
                    if (!hasAbs) {
                        lastAbs = ev.value;
                        hasAbs = true;
                        continue;
                    }
                    dx = ev.value - lastAbs;
                    lastAbs = ev.value;
                }

                if (std::abs(dx) > kMaxAbsDelta) {
                    detector.reset();
                    continue;
                }

                if (leftPressed && detector.feed(dx))
                    emit shakeDetected();
            }
        }
    }

    log.pop();
}
