#ifndef THEMEPROVIDER_H
#define THEMEPROVIDER_H

#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

class ThemeProvider
{
public:
    ThemeProvider();
    static QPalette light(){
        QPalette pal;

        // Backgrounds
        pal.setColor(QPalette::Window,        QColor("#FFFFFF"));
        pal.setColor(QPalette::Base,          QColor("#FFFFFF"));
        pal.setColor(QPalette::AlternateBase, QColor("#F2F0F7"));
        pal.setColor(QPalette::Button,        QColor("#E7E4ED"));

        // Text
        pal.setColor(QPalette::WindowText,    QColor("#000000"));
        pal.setColor(QPalette::Text,          QColor("#000000"));
        pal.setColor(QPalette::ButtonText,    QColor("#000000"));
        pal.setColor(QPalette::PlaceholderText, QColor("#615C6D"));
        pal.setColor(QPalette::BrightText,    QColor("#FFFFFF"));

        // Borders
        pal.setColor(QPalette::Light,         QColor("#FFFFFF"));
        pal.setColor(QPalette::Midlight,      QColor("#CFCAD9"));
        pal.setColor(QPalette::Mid,           QColor("#B7B1C4"));
        pal.setColor(QPalette::Dark,          QColor("#948CA3"));
        pal.setColor(QPalette::Shadow,        QColor("#C4BECF"));

        // Selection
        pal.setColor(QPalette::Highlight,     QColor("#D8CBF2"));
        pal.setColor(QPalette::HighlightedText, QColor("#000000"));
        pal.setColor(QPalette::Accent,        QColor("#7537F1"));

        // Links
        pal.setColor(QPalette::Link,          QColor("#005FB8"));
        pal.setColor(QPalette::LinkVisited,   QColor("#7B0796"));

        return pal;
    };

    static QPalette dark(){
        QPalette pal;

        // Backgrounds
        pal.setColor(QPalette::Window,        QColor("#141216"));
        pal.setColor(QPalette::Base,          QColor("#141216"));
        pal.setColor(QPalette::AlternateBase, QColor("#221F27"));
        pal.setColor(QPalette::Button,        QColor("#2B2732"));

        // Text
        pal.setColor(QPalette::WindowText,    QColor("#FFFFFF"));
        pal.setColor(QPalette::Text,          QColor("#FFFFFF"));
        pal.setColor(QPalette::ButtonText,    QColor("#FFFFFF"));
        pal.setColor(QPalette::PlaceholderText, QColor("#8A8694"));
        pal.setColor(QPalette::BrightText,    QColor("#FFFFFF"));

        // Borders
        pal.setColor(QPalette::Light,         QColor("#2F2A35"));
        pal.setColor(QPalette::Midlight,      QColor("#3A3540"));
        pal.setColor(QPalette::Mid,           QColor("#4A4453"));
        pal.setColor(QPalette::Dark,          QColor("#5A5364"));
        pal.setColor(QPalette::Shadow,        QColor("#2A2630"));

        // Selection
        pal.setColor(QPalette::Highlight,     QColor("#4C2D91"));
        pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
        pal.setColor(QPalette::Accent,        QColor("#9E6CFF"));

        // Links
        pal.setColor(QPalette::Link,          QColor("#4A90E2"));
        pal.setColor(QPalette::LinkVisited,   QColor("#B76CD8"));

        return pal;
    };

    static bool isDark()
    {
        return QGuiApplication::styleHints()->colorScheme()
               == Qt::ColorScheme::Dark;
    }

    static QPalette theme()
    {
        return isDark() ? ThemeProvider::dark() : ThemeProvider::light();
    }

};

#endif // THEMEPROVIDER_H
