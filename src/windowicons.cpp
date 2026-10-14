#include "windowicons.h"

#include "themeprovider.h"

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QWidget>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void WindowIcons::apply(QApplication &app, QWidget &window)
{
#if defined(Q_OS_MACOS)
    // Icon comes from the bundle's Tokri.icns.
    Q_UNUSED(app);
    Q_UNUSED(window);
#else
    const QIcon tray = [] {
        QPixmap pm(":/tray.png");
        const bool darkTaskbar = ThemeProvider::isTaskbarDark();
        if (darkTaskbar)
            return QIcon(pm);
        // On a light taskbar, render a crisp dark icon (#141216)
        QPixmap light(pm.size());
        light.fill(Qt::transparent);
        QPainter p(&light);
        p.drawPixmap(0, 0, pm);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(light.rect(), QColor("#141216"));
        p.end();
        return QIcon(light);
    }();

#if defined(Q_OS_LINUX)
    // One _NET_WM_ICON list: small entries are the titlebar icon, large
    // entries are the Alt+Tab/taskbar icon.
    app.setDesktopFileName("com.ysuraj.Tokri");
    QIcon windowIcon;
    windowIcon.addPixmap(tray.pixmap(16, 16));
    windowIcon.addPixmap(tray.pixmap(24, 24));
    const QIcon switcher(":/com.ysuraj.Tokri.png");
    windowIcon.addPixmap(switcher.pixmap(32, 32));
    windowIcon.addPixmap(switcher.pixmap(48, 48));
    windowIcon.addPixmap(switcher.pixmap(64, 64));
    windowIcon.addPixmap(switcher.pixmap(128, 128));
    windowIcon.addPixmap(switcher.pixmap(256, 256));
    app.setWindowIcon(windowIcon);
    window.setWindowIcon(windowIcon);
#else
    app.setWindowIcon(tray);
    window.setWindowIcon(tray);
#endif

#if defined(Q_OS_WIN)
    HWND hwnd = reinterpret_cast<HWND>(window.winId());
    if (!hwnd)
        return;

    static HICON previousSmall = nullptr;
    static HICON previousBig = nullptr;

    // Use the theme-adapted icon for both small (titlebar) and big (taskbar/Alt+Tab)
    HICON hSmall = tray.pixmap(16, 16).toImage().toHICON();
    HICON hBig   = tray.pixmap(48, 48).toImage().toHICON();

    SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hSmall));
    SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hBig));

    if (previousSmall)
        DestroyIcon(previousSmall);
    if (previousBig)
        DestroyIcon(previousBig);
    previousSmall = hSmall;
    previousBig = hBig;
#endif
#endif
}
