#include "fullscreenhelper.h"
#include "loghelpers.h"

#include <QWidget>
#include <QLibrary>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#endif

#if defined(Q_OS_LINUX) && defined(TOKRI_HAVE_DBUS)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QMap>
#include <QString>
#include <QVariant>
#endif

namespace FullscreenHelper {

void initPlatformPermissions()
{
#ifdef Q_OS_MACOS
    // Prompt the user for Accessibility permission on app startup if not yet granted
    const void *keys[] = { kAXTrustedCheckOptionPrompt };
    const void *values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (options) {
        Boolean trusted = AXIsProcessTrustedWithOptions(options);
        Logger::instance().log() << "macOS Accessibility permission trusted=" << (trusted ? true : false);
        CFRelease(options);
    }
#endif
}

#ifdef Q_OS_WIN
static bool isWindowsFullscreenOrGaming(QWidget *selfWindow)
{
    // 1. Direct DirectX / D3D game notification state
    QUERY_USER_NOTIFICATION_STATE notificationState;
    if (SUCCEEDED(SHQueryUserNotificationState(&notificationState))) {
        if (notificationState == QUNS_RUNNING_D3D_FULL_SCREEN ||
            notificationState == QUNS_PRESENTATION_MODE) {
            Logger::instance().log() << "Windows gaming state detected: QUNS=" << static_cast<int>(notificationState);
            return true;
        }
    }

    // 2. Foreground window rect check vs physical monitor bounds
    HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;

    // Do not suppress if foreground is Tokri itself
    if (selfWindow) {
        HWND selfHwnd = reinterpret_cast<HWND>(selfWindow->winId());
        if (foreground == selfHwnd)
            return false;
    }

    // Ignore desktop and shell windows
    if (foreground == GetDesktopWindow() || foreground == GetShellWindow())
        return false;

    char className[64] = {};
    if (GetClassNameA(foreground, className, sizeof(className))) {
        if (strcmp(className, "Progman") == 0 ||
            strcmp(className, "WorkerW") == 0 ||
            strcmp(className, "Shell_TrayWnd") == 0) {
            return false;
        }
    }

    if (IsIconic(foreground))
        return false;

    HMONITOR hMonitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    if (!hMonitor)
        return false;

    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(hMonitor, &mi))
        return false;

    RECT windowRect;
    if (!GetWindowRect(foreground, &windowRect))
        return false;

    // Check if the window bounds completely cover or exceed the monitor bounding box
    if (windowRect.left <= mi.rcMonitor.left &&
        windowRect.top <= mi.rcMonitor.top &&
        windowRect.right >= mi.rcMonitor.right &&
        windowRect.bottom >= mi.rcMonitor.bottom) {
        Logger::instance().log() << "Windows fullscreen window detected covering monitor bounds";
        return true;
    }

    return false;
}
#endif

#ifdef Q_OS_MACOS
static bool isMacFullscreenOrGaming(QWidget *selfWindow)
{
    Q_UNUSED(selfWindow);

    // 1. Exclusive fullscreen / captured display
    if (CGDisplayIsCaptured(CGMainDisplayID())) {
        Logger::instance().log() << "macOS captured display detected";
        return true;
    }

    // 2. Query focused application & window via Accessibility API
    AXUIElementRef systemWide = AXUIElementCreateSystemWide();
    if (!systemWide)
        return false;

    bool isFullscreen = false;
    AXUIElementRef focusedApp = nullptr;
    if (AXUIElementCopyAttributeValue(systemWide, kAXFocusedApplicationAttribute, reinterpret_cast<CFTypeRef *>(&focusedApp)) == kAXErrorSuccess && focusedApp) {
        AXUIElementRef focusedWindow = nullptr;
        if (AXUIElementCopyAttributeValue(focusedApp, kAXFocusedWindowAttribute, reinterpret_cast<CFTypeRef *>(&focusedWindow)) == kAXErrorSuccess && focusedWindow) {
            
            // Check native macOS AXFullScreen attribute
            CFBooleanRef axFullScreenVal = nullptr;
            if (AXUIElementCopyAttributeValue(focusedWindow, CFSTR("AXFullScreen"), reinterpret_cast<CFTypeRef *>(&axFullScreenVal)) == kAXErrorSuccess && axFullScreenVal) {
                if (CFBooleanGetValue(axFullScreenVal)) {
                    isFullscreen = true;
                }
                CFRelease(axFullScreenVal);
            }

            // Check if window bounds cover the main display (borderless / custom games)
            if (!isFullscreen) {
                AXValueRef posVal = nullptr;
                AXValueRef sizeVal = nullptr;
                if (AXUIElementCopyAttributeValue(focusedWindow, kAXPositionAttribute, reinterpret_cast<CFTypeRef *>(&posVal)) == kAXErrorSuccess && posVal &&
                    AXUIElementCopyAttributeValue(focusedWindow, kAXSizeAttribute, reinterpret_cast<CFTypeRef *>(&sizeVal)) == kAXErrorSuccess && sizeVal) {
                    CGPoint pos;
                    CGSize size;
                    if (AXValueGetValue(posVal, kAXValueTypeCGPoint, &pos) &&
                        AXValueGetValue(sizeVal, kAXValueTypeCGSize, &size)) {
                        CGRect displayBounds = CGDisplayBounds(CGMainDisplayID());
                        if (pos.x <= displayBounds.origin.x &&
                            pos.y <= displayBounds.origin.y &&
                            (pos.x + size.width) >= (displayBounds.origin.x + displayBounds.size.width) &&
                            (pos.y + size.height) >= (displayBounds.origin.y + displayBounds.size.height)) {
                            isFullscreen = true;
                        }
                    }
                }
                if (posVal) CFRelease(posVal);
                if (sizeVal) CFRelease(sizeVal);
            }

            CFRelease(focusedWindow);
        }
        CFRelease(focusedApp);
    }
    CFRelease(systemWide);

    if (isFullscreen) {
        Logger::instance().log() << "macOS fullscreen application detected";
    }

    return isFullscreen;
}
#endif

#ifdef Q_OS_LINUX
static bool isGameModeActive()
{
#if defined(TOKRI_HAVE_DBUS)
    auto checkBus = [](const QDBusConnection &bus) -> bool {
        if (!bus.isConnected())
            return false;
        QDBusInterface gm("com.feralinteractive.GameMode",
                          "/com/feralinteractive/GameMode",
                          "com.feralinteractive.GameMode",
                          bus);
        if (!gm.isValid())
            return false;
        QDBusReply<int> reply = gm.call("QueryStatus");
        if (reply.isValid() && reply.value() > 0)
            return true;
        return false;
    };

    if (checkBus(QDBusConnection::sessionBus())) {
        Logger::instance().log() << "Linux Feral GameMode active on session bus";
        return true;
    }
    if (checkBus(QDBusConnection::systemBus())) {
        Logger::instance().log() << "Linux Feral GameMode active on system bus";
        return true;
    }
#endif
    return false;
}

static bool isGnomeFullscreenActive()
{
#if defined(TOKRI_HAVE_DBUS)
    QDBusInterface introspect("org.gnome.Shell.Introspect",
                              "/org/gnome/Shell/Introspect",
                              "org.gnome.Shell.Introspect",
                              QDBusConnection::sessionBus());
    if (!introspect.isValid())
        return false;

    QDBusMessage reply = introspect.call("GetWindows");
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        const QVariant arg = reply.arguments().at(0);
        if (arg.canConvert<QVariantMap>()) {
            const QVariantMap windows = arg.toMap();
            for (auto it = windows.cbegin(); it != windows.cend(); ++it) {
                const QVariantMap props = it.value().toMap();
                if (props.value("has-focus").toBool() && props.value("is-fullscreen").toBool()) {
                    Logger::instance().log() << "GNOME Shell focused fullscreen window detected";
                    return true;
                }
            }
        } else if (arg.userType() == qMetaTypeId<QDBusArgument>()) {
            const QDBusArgument dbusArg = arg.value<QDBusArgument>();
            QMap<QString, QVariantMap> windows;
            dbusArg >> windows;
            for (auto it = windows.cbegin(); it != windows.cend(); ++it) {
                const QVariantMap &props = it.value();
                if (props.value("has-focus").toBool() && props.value("is-fullscreen").toBool()) {
                    Logger::instance().log() << "GNOME Shell focused fullscreen window detected (via DBusArgument)";
                    return true;
                }
            }
        }
    }
#endif
    return false;
}

static bool isX11FullscreenActive()
{
    // Dynamically load libX11 to query EWMH _NET_ACTIVE_WINDOW and _NET_WM_STATE_FULLSCREEN
    QLibrary libX11("X11");
    if (!libX11.load())
        return false;

    typedef void* (*XOpenDisplayFn)(const char*);
    typedef int (*XCloseDisplayFn)(void*);
    typedef unsigned long (*XRootWindowFn)(void*, int);
    typedef int (*XDefaultScreenFn)(void*);
    typedef unsigned long (*XInternAtomFn)(void*, const char*, int);
    typedef int (*XGetWindowPropertyFn)(void*, unsigned long, unsigned long, long, long, int, unsigned long,
                                        unsigned long*, int*, unsigned long*, unsigned long*, unsigned char**);
    typedef int (*XFreeFn)(void*);

    auto pXOpenDisplay = reinterpret_cast<XOpenDisplayFn>(libX11.resolve("XOpenDisplay"));
    auto pXCloseDisplay = reinterpret_cast<XCloseDisplayFn>(libX11.resolve("XCloseDisplay"));
    auto pXRootWindow = reinterpret_cast<XRootWindowFn>(libX11.resolve("XRootWindow"));
    auto pXDefaultScreen = reinterpret_cast<XDefaultScreenFn>(libX11.resolve("XDefaultScreen"));
    auto pXInternAtom = reinterpret_cast<XInternAtomFn>(libX11.resolve("XInternAtom"));
    auto pXGetWindowProperty = reinterpret_cast<XGetWindowPropertyFn>(libX11.resolve("XGetWindowProperty"));
    auto pXFree = reinterpret_cast<XFreeFn>(libX11.resolve("XFree"));

    if (!pXOpenDisplay || !pXCloseDisplay || !pXRootWindow || !pXDefaultScreen ||
        !pXInternAtom || !pXGetWindowProperty || !pXFree) {
        return false;
    }

    void *display = pXOpenDisplay(nullptr);
    if (!display)
        return false;

    bool isFullscreen = false;
    int screen = pXDefaultScreen(display);
    unsigned long root = pXRootWindow(display, screen);

    unsigned long netActiveWinAtom = pXInternAtom(display, "_NET_ACTIVE_WINDOW", 0);
    unsigned long netWmStateAtom = pXInternAtom(display, "_NET_WM_STATE", 0);
    unsigned long netWmStateFullscreenAtom = pXInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);

    unsigned long actualType = 0;
    int actualFormat = 0;
    unsigned long nItems = 0;
    unsigned long bytesAfter = 0;
    unsigned char *prop = nullptr;

    // Query active window from root
    if (pXGetWindowProperty(display, root, netActiveWinAtom, 0, 1, 0, 33 /* XA_WINDOW */,
                            &actualType, &actualFormat, &nItems, &bytesAfter, &prop) == 0 && prop) {
        if (nItems > 0) {
            unsigned long activeWindow = *reinterpret_cast<unsigned long*>(prop);
            pXFree(prop);
            prop = nullptr;

            if (activeWindow != 0) {
                if (pXGetWindowProperty(display, activeWindow, netWmStateAtom, 0, 1024, 0, 4 /* XA_ATOM */,
                                        &actualType, &actualFormat, &nItems, &bytesAfter, &prop) == 0 && prop) {
                    unsigned long *atoms = reinterpret_cast<unsigned long*>(prop);
                    for (unsigned long i = 0; i < nItems; ++i) {
                        if (atoms[i] == netWmStateFullscreenAtom) {
                            isFullscreen = true;
                            break;
                        }
                    }
                    pXFree(prop);
                    prop = nullptr;
                }
            }
        } else {
            pXFree(prop);
        }
    }

    pXCloseDisplay(display);

    if (isFullscreen) {
        Logger::instance().log() << "Linux X11 _NET_WM_STATE_FULLSCREEN detected on active window";
    }

    return isFullscreen;
}

static bool isLinuxFullscreenOrGaming(QWidget *selfWindow)
{
    Q_UNUSED(selfWindow);

    // 1. Check Feral GameMode (Steam Proton, Lutris, Heroic, native games)
    if (isGameModeActive())
        return true;

    // 2. Check GNOME Shell active window fullscreen status
    if (isGnomeFullscreenActive())
        return true;

    // 3. Check X11 EWMH active window fullscreen status (KDE, XFCE, tiling WMs, etc.)
    if (isX11FullscreenActive())
        return true;

    return false;
}
#endif

bool isForegroundFullscreenOrGaming(QWidget *selfWindow)
{
#ifdef Q_OS_WIN
    return isWindowsFullscreenOrGaming(selfWindow);
#elif defined(Q_OS_MACOS)
    return isMacFullscreenOrGaming(selfWindow);
#elif defined(Q_OS_LINUX)
    return isLinuxFullscreenOrGaming(selfWindow);
#else
    Q_UNUSED(selfWindow);
    return false;
#endif
}

} // namespace FullscreenHelper
