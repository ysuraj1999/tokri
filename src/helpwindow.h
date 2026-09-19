#ifndef HELPWINDOW_H
#define HELPWINDOW_H

#include <QDialog>

class HelpWindow : public QDialog
{
    Q_OBJECT
public:
    explicit HelpWindow(QWidget *parent = nullptr);
};

#endif // HELPWINDOW_H
