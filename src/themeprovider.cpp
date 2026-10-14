#include "themeprovider.h"

#include <QColor>
#include <QByteArray>
#include <QVariant>

#if defined(Q_OS_LINUX) && defined(TOKRI_HAVE_DBUS)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#endif

ThemeProvider::ThemeProvider() { }

QPalette ThemeProvider::light()
{
    QPalette pal;

    // Backgrounds
    pal.setColor(QPalette::Window,        QColor("#FFFFFF"));
    pal.setColor(QPalette::Base,          QColor("#FFFFFF"));
    pal.setColor(QPalette::AlternateBase, QColor("#F5F3FA"));
    pal.setColor(QPalette::Button,        QColor("#F0EDF5"));

    // Text
    pal.setColor(QPalette::WindowText,    QColor("#1A181E"));
    pal.setColor(QPalette::Text,          QColor("#1A181E"));
    pal.setColor(QPalette::ButtonText,    QColor("#1A181E"));
    pal.setColor(QPalette::PlaceholderText, QColor("#7E788A"));
    pal.setColor(QPalette::BrightText,    QColor("#FFFFFF"));

    // Borders
    pal.setColor(QPalette::Light,         QColor("#FFFFFF"));
    pal.setColor(QPalette::Midlight,      QColor("#E8E5EE"));
    pal.setColor(QPalette::Mid,           QColor("#D0CBD8"));
    pal.setColor(QPalette::Dark,          QColor("#9E97AA"));
    pal.setColor(QPalette::Shadow,        QColor("#C4BECF"));

    // Selection
    pal.setColor(QPalette::Highlight,     QColor("#E4DCF8"));
    pal.setColor(QPalette::HighlightedText, QColor("#1A181E"));
    pal.setColor(QPalette::Accent,        QColor("#7537F1"));

    // Links
    pal.setColor(QPalette::Link,          QColor("#005FB8"));
    pal.setColor(QPalette::LinkVisited,   QColor("#7B0796"));

    return pal;
}

QPalette ThemeProvider::dark()
{
    QPalette pal;

    // Backgrounds
    pal.setColor(QPalette::Window,        QColor("#141216"));
    pal.setColor(QPalette::Base,          QColor("#141216"));
    pal.setColor(QPalette::AlternateBase, QColor("#1E1B22"));
    pal.setColor(QPalette::Button,        QColor("#26222C"));

    // Text
    pal.setColor(QPalette::WindowText,    QColor("#EDEDF2"));
    pal.setColor(QPalette::Text,          QColor("#EDEDF2"));
    pal.setColor(QPalette::ButtonText,    QColor("#EDEDF2"));
    pal.setColor(QPalette::PlaceholderText, QColor("#8E8899"));
    pal.setColor(QPalette::BrightText,    QColor("#FFFFFF"));

    // Borders
    pal.setColor(QPalette::Light,         QColor("#2B2732"));
    pal.setColor(QPalette::Midlight,      QColor("#383241"));
    pal.setColor(QPalette::Mid,           QColor("#484153"));
    pal.setColor(QPalette::Dark,          QColor("#5D556B"));
    pal.setColor(QPalette::Shadow,        QColor("#100E12"));

    // Selection
    pal.setColor(QPalette::Highlight,     QColor("#4C2D91"));
    pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    pal.setColor(QPalette::Accent,        QColor("#9E6CFF"));

    // Links
    pal.setColor(QPalette::Link,          QColor("#4A90E2"));
    pal.setColor(QPalette::LinkVisited,   QColor("#B76CD8"));

    return pal;
}

bool ThemeProvider::isDark()
{
#if defined(Q_OS_WIN)
    DWORD value = 1;
    DWORD size = sizeof(value);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LONG res = RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                                     reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS) {
            return (value == 0);
        }
    }
#endif

#if defined(Q_OS_LINUX) && defined(TOKRI_HAVE_DBUS)
    // Modern Linux desktop environments (GNOME 42+, KDE Plasma 6) report appearance
    // through the XDG Desktop Portal Settings interface.
    QDBusInterface portal("org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.portal.Settings",
                          QDBusConnection::sessionBus());
    if (portal.isValid()) {
        QDBusMessage reply = portal.call("Read", "org.freedesktop.appearance", "color-scheme");
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            QVariant v = reply.arguments().at(0).value<QDBusVariant>().variant();
            uint scheme = v.toUInt();
            if (scheme == 1) // 1 = Prefer dark
                return true;
            if (scheme == 2) // 2 = Prefer light
                return false;
        }
    }
#endif

    if (QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }

    return false;
}

bool ThemeProvider::isTaskbarDark()
{
#if defined(Q_OS_WIN)
    DWORD value = 1;
    DWORD size = sizeof(value);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LONG res = RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr,
                                     reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS) {
            return (value == 0);
        }
    }
#endif

#if defined(Q_OS_LINUX)
    // On GNOME and Ubuntu, the top bar / panel is dark (#131313) even when the app theme is light.
    const QByteArray desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    if (desktop.contains("gnome") || desktop.contains("ubuntu")) {
        return true;
    }
#endif

    return isDark();
}

QPalette ThemeProvider::theme()
{
    return isDark() ? ThemeProvider::dark() : ThemeProvider::light();
}
