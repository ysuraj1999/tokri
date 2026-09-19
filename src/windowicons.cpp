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
        if (ThemeProvider::isDark())
            return QIcon(pm);
        QPixmap dark(pm.size());
        dark.fill(Qt::transparent);
        QPainter p(&dark);
        p.drawPixmap(0, 0, pm);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(dark.rect(), QColor("#141216"));
        p.end();
        return QIcon(dark);
    }();

#if defined(Q_OS_LINUX)
    // One _NET_WM_ICON list: small entries are the titlebar icon, large
    // entries are the Alt+Tab/taskbar icon.
    app.setDesktopFileName("net.surajyadav.Tokri");
    QIcon windowIcon;
    windowIcon.addPixmap(tray.pixmap(16, 16));
    windowIcon.addPixmap(tray.pixmap(24, 24));
    const QIcon switcher(":/net.surajyadav.Tokri.png");
    windowIcon.addPixmap(switcher.pixmap(32, 32));
    windowIcon.addPixmap(switcher.pixmap(48, 48));
    windowIcon.addPixmap(switcher.pixmap(64, 64));
    windowIcon.addPixmap(switcher.pixmap(128, 128));
    windowIcon.addPixmap(switcher.pixmap(256, 256));
    app.setWindowIcon(windowIcon);
#else
    app.setWindowIcon(tray);
#endif

#if defined(Q_OS_WIN)
    QWindow *handle = window.windowHandle();
    if (!handle)
        return;
    HWND hwnd = reinterpret_cast<HWND>(handle->winId());
    if (!hwnd)
        return;

    static HICON previousSmall = nullptr;
    static HICON previousBig = nullptr;

    const QIcon ico(":/net.surajyadav.Tokri.ico");
    const QIcon &big = ico.isNull() ? tray : ico;

    HICON hSmall = tray.pixmap(16, 16).toImage().toHICON();
    HICON hBig = big.pixmap(256, 256).toImage().toHICON();

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
