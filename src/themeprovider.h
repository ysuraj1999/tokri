#ifndef THEMEPROVIDER_H
#define THEMEPROVIDER_H

#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class ThemeProvider
{
public:
    ThemeProvider();

    static QPalette light();
    static QPalette dark();
    static bool isDark();
    static bool isTaskbarDark();
    static QPalette theme();
};

#endif // THEMEPROVIDER_H
