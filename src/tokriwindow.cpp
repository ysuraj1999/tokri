#include "tokriwindow.h"
#include "./ui_tokriwindow.h"
#include "dropawarefilesystemmodel.h"
#include "filereveal.h"
#include "fullscreenhelper.h"
#include "loghelpers.h"
#include "standardpaths.h"
#include "themeprovider.h"
#include "thumbnaildelegate.h"
#include "thumbnaillayout.h"
#include "windowicons.h"
#include <QDir>
#include <QMenu>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QScrollBar>
#include <QUrl>
#include <QToolButton>
#include <QHBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#endif

TokriWindow::TokriWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TokriWindow)
{
    ui->setupUi(this);

    // Add Exit action as the last item in the View menu
    ui->menuView->addSeparator();
    QAction *exitAction = ui->menuView->addAction(tr("E&xit"));
    connect(exitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    init();
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);

    // Build the frameless top header bar (drag area + small X button)
    mHeaderBar = new QWidget(this);
    mHeaderBar->setFixedHeight(22);
    mHeaderBar->setCursor(Qt::SizeAllCursor); // Visual feedback that the header area is draggable

    auto *headerLayout = new QHBoxLayout(mHeaderBar);
    headerLayout->setContentsMargins(0, 0, 2, 0);
    headerLayout->setSpacing(0);

    // Centered pill / drag handle line (resembling picture)
    headerLayout->addStretch(1);

    mDragHandle = new QFrame(mHeaderBar);
    mDragHandle->setFixedHeight(4);
    mDragHandle->setFixedWidth(64); // ~20% of window width, pill-shaped horizontal drag bar
    mDragHandle->setCursor(Qt::SizeAllCursor);
    headerLayout->addWidget(mDragHandle, 0, Qt::AlignCenter);

    headerLayout->addStretch(1);

    mCloseButton = new QToolButton(mHeaderBar);
    mCloseButton->setFixedSize(18, 18);
    mCloseButton->setText(QStringLiteral("✕"));
    mCloseButton->setCursor(Qt::PointingHandCursor);
    mCloseButton->setToolTip(tr("Hide"));
    connect(mCloseButton, &QToolButton::clicked, this, &TokriWindow::sleep);

    headerLayout->addWidget(mCloseButton);

    // Insert header bar at the top of the central layout (above the list view)
    auto *centralLayout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
    if (centralLayout) {
        centralLayout->insertWidget(0, mHeaderBar);
    }

    ui->centralwidget->layout()->setContentsMargins(6, 4, 6, 6);
    ui->centralwidget->layout()->setSpacing(2);
    applyTheme();

    // 1-minute inactivity timer
    mInactivityTimer.setInterval(60000); // 1 minute (60,000 ms)
    mInactivityTimer.setSingleShot(true);
    connect(&mInactivityTimer, &QTimer::timeout, this, &TokriWindow::onInactivityTimeout);

    // Filter events across Tokri to detect Alt key and reset inactivity timer
    qApp->installEventFilter(this);

    // Hide window menu and titlebar buttons by default
    setChromeVisible(false);

    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->listView->setItemDelegate(new ThumbnailDelegate(ui->listView));

    ui->listView->setAcceptDrops(true);
    ui->listView->setFrameShape(QFrame::NoFrame);
    ui->listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listView->setDragDropMode(QAbstractItemView::DragDrop);
    ui->listView->setDragEnabled(true);
    ui->listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->listView->setTextElideMode(Qt::ElideMiddle);
    ui->listView->setResizeMode(QListView::Adjust);
    ui->listView->setLayoutMode(QListView::Batched);

    connect(ui->listView, &QListView::doubleClicked,
            this, [this](const QModelIndex &idx){
                if (!idx.isValid())
                    return;

                const QString filePath =
                    idx.data(QFileSystemModel::FileInfoRole)
                        .value<QFileInfo>()
                        .filePath();
                openItem(filePath);
    });

    ui->listView->setViewMode(QListView::IconMode);
    ui->listView->setIconSize({ThumbnailLayout::ThumbnailSize,
                               ThumbnailLayout::ThumbnailSize});
    ui->listView->setGridSize(
        {ThumbnailLayout::TileWidth,
         ThumbnailLayout::TileHeight(ui->listView->fontMetrics().height())});
    ui->listView->setFlow(QListView::LeftToRight);
    ui->listView->setWrapping(true);
    ui->listView->setUniformItemSizes(true);
    ui->listView->setSpacing(8);
    ui->listView->setMouseTracking(true);
    ui->listView->setFocusPolicy(Qt::StrongFocus);
    ui->listView->setDropIndicatorShown(false);
    ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

    applyColumnSizing();

    connect(ui->listView, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                Logger &log = Logger::instance();
                log.push("contextMenu");

                auto *view = ui->listView;
                auto *sel  = view->selectionModel();
                const auto selected = sel->selectedIndexes();
                const int count = selected.size();

                QMenu menu;
                menu.setPalette(this->palette());

                QAction *open = nullptr, *reveal = nullptr, *rename = nullptr;
                QAction *copy = nullptr, *del = nullptr, *selectAll = nullptr;
                QAction *paste = nullptr;

                if (count > 0) {
                    open = menu.addAction("&Open");
                    copy = menu.addAction("&Copy");
#ifdef Q_OS_WIN
                    del  = menu.addAction("&Delete");
#else
                    del  = menu.addAction("Move to &Trash");
#endif
                }
                if (count == 1) {
#ifdef Q_OS_MACOS
                    reveal= menu.addAction("Reveal in &Finder");
#elif defined(Q_OS_WIN)
                    reveal= menu.addAction("Open &file location");
#else
                    reveal= menu.addAction("Open Containing &Folder");
#endif
                    rename= menu.addAction("&Rename");
                }

                paste = menu.addAction("&Paste");
                paste->setEnabled(DropAwareFileSystemModel::isPasteable(
                    QGuiApplication::clipboard()->mimeData()));
                selectAll = menu.addAction("Select &All");

                QAction *chosen = menu.exec(view->viewport()->mapToGlobal(pos));
                log.log() << "count=" << count
                          << "chosen=" << (chosen ? chosen->text() : QString("<none>"));
                if (!chosen) {
                    log.pop();
                    return;
                }

                auto fileInfoAt = [](const QModelIndex &idx) {
                    return idx.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();
                };

                if (chosen == paste) {
                    emit pasteRequested();
                    log.pop();
                    return;
                }

                if (chosen == selectAll) {
                    this->selectAll();
                    log.pop();
                    return;
                }

                if (chosen == open) {
                    openSelection();
                    log.pop();
                    return;
                }

                if (count == 1 && chosen == reveal) {
                    FileReveal::revealInFileManager(
                        fileInfoAt(selected[0]).absoluteFilePath());
                    log.pop();
                    return;
                }

                if (count == 1 && chosen == rename) {
                    renameSelection();
                    log.pop();
                    return;
                }

                if (chosen == copy) {
                    copySelection();
                    log.pop();
                    return;
                }

                if (chosen == del) {
                    deleteSelection();
                }

                log.pop();
            });

}

TokriWindow::~TokriWindow()
{
    delete ui;
}

Ui::TokriWindow *TokriWindow::uiHandle()
{
    return ui;
}

void TokriWindow::sleep()
{
    Logger &log = Logger::instance();
    log.push("sleep");
    log.log() << "-> hide";

    mInactivityTimer.stop();
    hide();

    log.pop();
}

void TokriWindow::wakeUp()
{
    Logger &log = Logger::instance();
    log.push("wakeUp");

    // Check and apply active system theme the moment the window opens
    applyTheme();

    // Hide window menu and titlebar buttons by default
    setChromeVisible(false);

    // Reset 1-minute inactivity countdown
    resetInactivityTimer();

    const bool minimized = isMinimized();
    const bool hidden = !isVisible();

    log.log() << "minimized=" << minimized
              << "hidden=" << hidden;

    if (minimized || hidden) {
        moveNearCursor();
    }

    if (minimized) {
        showNormal();
    } else if (hidden) {
        show();
    }

    raise();
    activateWindow();
    ui->listView->setFocus();

    log.pop();
}

void TokriWindow::init()
{
    QString tokriDir = StandardPaths::getPath(StandardPaths::TokriDir);
    QDir dir(tokriDir);
    if (!dir.exists()){
        bool success = dir.mkpath(tokriDir);
        if (!success){
            // FIXME handle error
        }
    }
}

void TokriWindow::applyColumnSizing()
{
    using namespace ThumbnailLayout;

    int left = 0, top = 0, right = 0, bottom = 0;
    ui->centralwidget->layout()->getContentsMargins(&left, &top,
                                                    &right, &bottom);

    const int scrollBar =
        ui->listView->verticalScrollBar()->sizeHint().width();

    const int width = MinViewportWidth + scrollBar + TileGap + left + right;
    setMinimumWidth(width);
    resize(width, height());
}

void TokriWindow::moveNearCursor()
{
    const QPoint cursor = QCursor::pos();
    const QSize  winSize = size();
    QPoint p(cursor.x() + 20, cursor.y() + 20);

    QScreen *targetScreen = QGuiApplication::screenAt(cursor);
    if (!targetScreen)
        targetScreen = QGuiApplication::primaryScreen();
    const QRect screen = targetScreen ? targetScreen->availableGeometry()
                                      : QRect(0, 0, 1920, 1080);

    if (p.x() + winSize.width() > screen.right())
        p.setX(screen.right() - winSize.width());
    if (p.y() + winSize.height() > screen.bottom())
        p.setY(screen.bottom() - winSize.height());
    if (p.x() < screen.left())
        p.setX(screen.left());
    if (p.y() < screen.top())
        p.setY(screen.top());

    move(p);
}

void TokriWindow::onShakeDetect()
{
    Logger &log = Logger::instance();
    log.push("onShakeDetect");
    log.log() << "shake";

    if (FullscreenHelper::isForegroundFullscreenOrGaming(this)) {
        log.log() << "Shake ignored: foreground application is fullscreen or gaming";
        log.pop();
        return;
    }

    wakeUp();

    log.pop();
}


void TokriWindow::openItem(QString filePath) {
    Logger &log = Logger::instance();
    log.push("openItem");
    log.log() << "path=" << filePath;

    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));

    log.pop();
}

void TokriWindow::deleteSelection()
{
    Logger &log = Logger::instance();
    log.push("delete");

    auto *view = ui->listView;
    auto *sel = view->selectionModel();
    if (!sel) {
        log.pop();
        return;
    }

    const auto selected = sel->selectedIndexes();
    for (const auto &idx : selected) {
        if (!idx.isValid())
            continue;

        const QFileInfo fi =
            idx.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();
        if (!fi.exists())
            continue;

        log.log() << "trash path=" << fi.absoluteFilePath()
                  << "ok=" << QFile::moveToTrash(fi.absoluteFilePath());
    }

    log.pop();
}

void TokriWindow::renameSelection()
{
    Logger &log = Logger::instance();
    log.push("rename");

    auto *view = ui->listView;
    auto *sel = view->selectionModel();
    if (!sel) {
        log.pop();
        return;
    }

    const auto selected = sel->selectedIndexes();
    if (selected.size() != 1 || !selected[0].isValid()) {
        log.log() << "rename skipped count=" << selected.size();
        log.pop();
        return;
    }

    log.log() << "rename path="
              << selected[0].data(QFileSystemModel::FileInfoRole)
                     .value<QFileInfo>().filePath();
    view->edit(selected[0]);

    log.pop();
}

void TokriWindow::selectAll()
{
    Logger &log = Logger::instance();
    log.push("selectAll");

    ui->listView->selectAll();

    log.pop();
}

QList<QUrl> TokriWindow::selectedUrls() const
{
    QList<QUrl> urls;

    auto *sel = ui->listView->selectionModel();
    if (!sel)
        return urls;

    const auto selected = sel->selectedIndexes();
    for (const auto &idx : selected) {
        if (!idx.isValid())
            continue;

        const QFileInfo fi =
            idx.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();
        if (fi.exists())
            urls << QUrl::fromLocalFile(fi.absoluteFilePath());
    }

    return urls;
}

void TokriWindow::copySelection()
{
    Logger &log = Logger::instance();
    log.push("copySelection");

    const QList<QUrl> urls = selectedUrls();
    log.log() << "urls=" << urls;
    if (!urls.isEmpty()) {
        auto *mime = new QMimeData;
        mime->setUrls(urls);
        QGuiApplication::clipboard()->setMimeData(mime);
    }

    log.pop();
}

void TokriWindow::openSelection()
{
    Logger &log = Logger::instance();
    log.push("openSelection");

    auto *sel = ui->listView->selectionModel();
    if (!sel) {
        log.pop();
        return;
    }

    const auto selected = sel->selectedIndexes();
    log.log() << "selection count=" << selected.size();

    for (const QModelIndex &idx : selected) {
        if (!idx.isValid())
            continue;

        const QString filePath =
            idx.data(QFileSystemModel::FileInfoRole).value<QFileInfo>().filePath();
        log.log() << "path=" << filePath;
        openItem(filePath);
    }

    log.pop();
}

void TokriWindow::closeEvent(QCloseEvent *e)
{
    Logger &log = Logger::instance();
    log.push("closeEvent");
    log.log() << "-> ignore & sleep";

    e->ignore();
    sleep();

    log.pop();
}

void TokriWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    // Check and apply active system theme the moment the window opens
    applyTheme();

    // Reset 1-minute inactivity countdown
    resetInactivityTimer();
}

void TokriWindow::setChromeVisible(bool visible)
{
    mChromeVisible = visible;
    if (ui->menuBar)
        ui->menuBar->setVisible(visible);

    // Keep app name "Tokri"
    setWindowTitle(QStringLiteral("Tokri"));

    const bool wasVisible = isVisible();
    const QPoint prevPos = pos();
    const QSize prevSize = size();

    if (visible) {
        // Restore standard window decorations (title bar, minimize, maximize, close buttons)
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                       Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                       Qt::WindowCloseButtonHint);
    } else {
        // Frameless mode (content only, no title bar or border buttons)
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    }

    if (wasVisible) {
        show();
        move(prevPos);
        resize(prevSize);
#ifdef Q_OS_WIN
        updateWindowsTitleBarTheme(ThemeProvider::isDark());
#endif
    }

    // When standard window decorations & menu bar are visible, hide the custom frameless header
    // When frameless (chrome hidden), show the custom header bar with the small X button
    if (mHeaderBar)
        mHeaderBar->setVisible(!visible);

    setWindowButtonsVisible(visible);
}

void TokriWindow::toggleChrome()
{
    setChromeVisible(!mChromeVisible);
}

void TokriWindow::setWindowButtonsVisible(bool visible)
{
#if defined(Q_OS_WIN)
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if (visible) {
        style |= WS_SYSMENU;
    } else {
        style &= ~WS_SYSMENU;
    }
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#elif defined(Q_OS_MACOS)
    // Note for macOS:
    // [[window standardWindowButton:NSWindowCloseButton] setHidden:!visible];
#elif defined(Q_OS_LINUX)
    // Note for Linux:
    // Handled by Qt::FramelessWindowHint above.
#endif
}

void TokriWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWindow *win = windowHandle();
        if (!win && window()) {
            win = window()->windowHandle();
        }
        if (win && win->startSystemMove()) {
            event->accept();
            return;
        }
        mDragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void TokriWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && !mDragPosition.isNull()) {
        move(event->globalPosition().toPoint() - mDragPosition);
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
}

void TokriWindow::resetInactivityTimer()
{
    if (isVisible()) {
        mInactivityTimer.start();
    }
}

void TokriWindow::onInactivityTimeout()
{
    // Do not sleep if a modal dialog (e.g. About, Shortcuts) or context menu is active
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) {
        resetInactivityTimer();
        return;
    }

    Logger::instance().log() << "Inactivity timeout (1 minute) -> sleep";
    sleep();
}

bool TokriWindow::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (widget && (widget == this || this->isAncestorOf(widget))) {
        // Drag window by clicking and dragging on mHeaderBar (the top drag area)
        if (mHeaderBar && (watched == mHeaderBar || mHeaderBar->isAncestorOf(widget))) {
            if (widget != mCloseButton) {
                if (event->type() == QEvent::MouseButtonPress) {
                    auto *me = static_cast<QMouseEvent *>(event);
                    if (me->button() == Qt::LeftButton) {
                        resetInactivityTimer();
                        QWindow *win = windowHandle();
                        if (!win && window()) {
                            win = window()->windowHandle();
                        }
                        if (win && win->startSystemMove()) {
                            return true;
                        }
                        mDragPosition = me->globalPosition().toPoint() - frameGeometry().topLeft();
                        return true;
                    }
                } else if (event->type() == QEvent::MouseMove) {
                    auto *me = static_cast<QMouseEvent *>(event);
                    if (me->buttons() & Qt::LeftButton && !mDragPosition.isNull()) {
                        move(me->globalPosition().toPoint() - mDragPosition);
                        resetInactivityTimer();
                        return true;
                    }
                } else if (event->type() == QEvent::MouseButtonRelease) {
                    mDragPosition = QPoint();
                }
            }
        }

        // Toggle window menu and window buttons on Alt key tap
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Alt) {
                mAltKeyPressedAlone = true;
            } else {
                mAltKeyPressedAlone = false;
            }
        } else if (event->type() == QEvent::KeyRelease) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Alt && mAltKeyPressedAlone && !ke->isAutoRepeat()) {
                mAltKeyPressedAlone = false;
                toggleChrome();
                resetInactivityTimer();
                return true;
            } else {
                mAltKeyPressedAlone = false;
            }
        }

        // Reset 1-minute inactivity countdown on any user interaction
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::DragEnter:
        case QEvent::DragMove:
        case QEvent::Drop:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::WindowActivate:
            resetInactivityTimer();
            break;
        default:
            break;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void TokriWindow::applyTheme()
{
    const bool dark = ThemeProvider::isDark();
    const QPalette pal = ThemeProvider::theme();

    QApplication::setPalette(pal);
    setPalette(pal);

    if (ui->centralwidget)
        ui->centralwidget->setPalette(pal);

    if (ui->listView) {
        ui->listView->setPalette(pal);
        ui->listView->viewport()->setPalette(pal);
    }

    if (ui->menuBar)
        ui->menuBar->setPalette(pal);

    if (ui->menuView)
        ui->menuView->setPalette(pal);

    if (mCloseButton) {
        if (dark) {
            mCloseButton->setStyleSheet(
                "QToolButton {"
                "  background-color: rgba(255, 255, 255, 0.15);"
                "  color: #cccccc;"
                "  border-radius: 9px;"
                "  border: none;"
                "  font-size: 11px;"
                "  font-weight: bold;"
                "}"
                "QToolButton:hover {"
                "  background-color: rgba(255, 80, 80, 0.75);"
                "  color: #ffffff;"
                "}"
                "QToolButton:pressed {"
                "  background-color: rgba(200, 50, 50, 0.9);"
                "  color: #ffffff;"
                "}"
            );
        } else {
            mCloseButton->setStyleSheet(
                "QToolButton {"
                "  background-color: rgba(0, 0, 0, 0.08);"
                "  color: #555555;"
                "  border-radius: 9px;"
                "  border: none;"
                "  font-size: 11px;"
                "  font-weight: bold;"
                "}"
                "QToolButton:hover {"
                "  background-color: rgba(230, 60, 60, 0.85);"
                "  color: #ffffff;"
                "}"
                "QToolButton:pressed {"
                "  background-color: rgba(190, 40, 40, 0.95);"
                "  color: #ffffff;"
                "}"
            );
        }
    }

    if (mDragHandle) {
        if (dark) {
            mDragHandle->setStyleSheet(
                "QFrame {"
                "  background-color: rgba(255, 255, 255, 0.40);"
                "  border-radius: 2px;"
                "}"
            );
        } else {
            mDragHandle->setStyleSheet(
                "QFrame {"
                "  background-color: rgba(0, 0, 0, 0.22);"
                "  border-radius: 2px;"
                "}"
            );
        }
    }

#ifdef Q_OS_WIN
    updateWindowsTitleBarTheme(dark);
#endif
    // Note for macOS:
    // macOS AppKit manages window title bar chrome automatically based on NSAppearance.
    // Note for Linux:
    // Linux window managers (KWin, Mutter) theme title bars via XDG/portal settings.

    WindowIcons::apply(*qApp, *this);

    emit themeChanged(dark);

    update();
    if (ui->listView)
        ui->listView->viewport()->update();
}

#ifdef Q_OS_WIN
void TokriWindow::updateWindowsTitleBarTheme(bool dark)
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;

    BOOL darkMode = dark ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE is supported on Windows 11 and Windows 10 build 19041+
    if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode)))) {
        // Fallback for Windows 10 build 1809 - 1909
        DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));
    }

    // Apply native modern rounded corners on Windows 11 (works for frameless and styled windows)
    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
}
#endif

bool TokriWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_SETTINGCHANGE) {
            // Windows sends WM_SETTINGCHANGE when system theme (AppsUseLightTheme) changes
            applyTheme();
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

