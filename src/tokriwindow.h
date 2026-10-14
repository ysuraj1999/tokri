#ifndef TOKRIWINDOW_H
#define TOKRIWINDOW_H

#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QFrame>
#include <QList>
#include <QListView>
#include <QMainWindow>
#include <QMimeData>
#include <QTimer>
#include <QToolButton>
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
    void applyTheme();
    void setChromeVisible(bool visible);
    void toggleChrome();
    void resetInactivityTimer();
    void onShakeDetect();
    void deleteSelection();
    void renameSelection();
    void copySelection();
    void openSelection();
    void selectAll();

signals:
    void pasteRequested();
    void themeChanged(bool isDark);

protected:
    void showEvent(QShowEvent *e) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    Ui::TokriWindow *ui;

    void init();
    void applyColumnSizing();
    void moveNearCursor();
    void openItem(QString filePath);
    QList<QUrl> selectedUrls() const;
    void setWindowButtonsVisible(bool visible);
    void onInactivityTimeout();

#ifdef Q_OS_WIN
    void updateWindowsTitleBarTheme(bool dark);
#endif

    bool mChromeVisible = false;
    bool mAltKeyPressedAlone = false;
    QPoint mDragPosition;
    QTimer mInactivityTimer;

    QWidget *mHeaderBar = nullptr;
    QFrame *mDragHandle = nullptr;
    QToolButton *mCloseButton = nullptr;
};
#endif // TOKRIWINDOW_H
