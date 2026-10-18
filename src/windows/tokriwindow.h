#ifndef TOKRIWINDOW_H
#define TOKRIWINDOW_H

#include <QListView>
#include <QMainWindow>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QCloseEvent>
#include <QList>
#include <QUrl>

QT_BEGIN_NAMESPACE
namespace Ui {
class TokriWindow;
}
QT_END_NAMESPACE

class TokriWindow : public QMainWindow
{
    Q_OBJECT

public:
    TokriWindow(QWidget *parent = nullptr);
    ~TokriWindow();
    Ui::TokriWindow* uiHandle();
    void sleep();
    void wakeUp();

public slots:
    void onShakeDetect();
    void deleteSelection();
    void renameSelection();
    void copySelection();
    void openSelection();
    void selectAll();

signals:
    void pasteRequested();

private:
    Ui::TokriWindow *ui;

    void init();
    void setupListView();
    void showContextMenu(const QPoint &pos);
    void applyColumnSizing();
    void moveNearCursor();
    void closeEvent(QCloseEvent *e);
    void openItem(QString filePath);
    QList<QUrl> selectedUrls() const;
};
#endif // TOKRIWINDOW_H
