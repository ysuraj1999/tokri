#include "dropawarefilesystemmodel.h"
#include "drophandler.h"
#include "aboutwindow.h"
#include "helpwindow.h"
#include "loghelpers.h"
#include "logwindow.h"
#include "themeprovider.h"
#include "tokriwindow.h"
#include "sortfilterproxy.h"
#include "thumbnailproxymodel.h"
#include "ui_tokriwindow.h"
#include "standardnames.h"
#include "standardpaths.h"
#include "windowicons.h"
#include "fullscreenhelper.h"

#ifdef Q_OS_WIN
#include "windowsdragshakedetector.h"
#endif

#ifdef Q_OS_LINUX
#include "linuxdragshakedetector.h"
#endif

#ifdef Q_OS_MACOS
#include "macosdragshakedetector.h"
#endif

#include <QAbstractItemView>
#include <QAbstractProxyModel>
#include <QClipboard>
#include <QItemSelectionModel>
#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QLineEdit>
#include <QMimeData>
#include <QQueue>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QThread>
#include <QVBoxLayout>
#include <QLocalServer>
#include <QLocalSocket>
#include <QShortcut>
#include <QLockFile>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStyleFactory>
#include <QIcon>

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Allow native Wayland when running on a Wayland compositor session (e.g. GNOME/KDE Wayland),
    // eliminating XWayland drag-and-drop bridging issues with native Wayland apps like Firefox and Nautilus.
    // Falls back to xcb on pure X11 sessions, and respects any user-set QT_QPA_PLATFORM.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        const bool isWayland = qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
        if (!isWayland) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        }
    }
#endif
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);
    a.setApplicationName("Tokri");
#ifdef TOKRI_VERSION
    a.setApplicationVersion(TOKRI_VERSION);
#endif

#ifdef Q_OS_WIN
    a.setStyle(QStyleFactory::create("Fusion"));
#endif

    a.setPalette(ThemeProvider::theme());

    LogSink::install();
    QThread::currentThread()->setObjectName("main");
    Logger::instance().log() << "Tokri starting";

    FullscreenHelper::initPlatformPermissions();

    QLocalServer server;
    TokriWindow tokriWindow;
    WindowIcons::apply(a, tokriWindow);

    LogWindow *logWindow = new LogWindow(&tokriWindow);
    QAction *logsAction = tokriWindow.uiHandle()->actionLogs;
    QObject::connect(logsAction, &QAction::triggered,
                     logWindow, [logWindow] {
                         logWindow->show();
                         logWindow->raise();
                         logWindow->activateWindow();
                     });

    HelpWindow *helpWindow = new HelpWindow(&tokriWindow);
    QAction *helpAction = tokriWindow.uiHandle()->actionHelp;
    helpAction->setShortcut(QKeySequence(Qt::Key_F1));
    QObject::connect(helpAction, &QAction::triggered,
                     helpWindow, [helpWindow] {
                         helpWindow->raise();
                         helpWindow->activateWindow();
                         helpWindow->exec();
                     });

    AboutWindow *aboutWindow = new AboutWindow(&tokriWindow);
    QAction *aboutAction = tokriWindow.uiHandle()->actionAbout;
    QObject::connect(aboutAction, &QAction::triggered,
                     aboutWindow, [aboutWindow] {
                         aboutWindow->raise();
                         aboutWindow->activateWindow();
                         aboutWindow->exec();
                     });

    // Single Instance
    const QString lockFilePath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                             + "/"
                             + StandardNames::get(StandardNames::LockFile);
    static QLockFile lockFile(lockFilePath);
    lockFile.setStaleLockTime(0);
    QString localServerName = StandardNames::get(StandardNames::LocalServer);

    if (!lockFile.tryLock()) {
        QLocalSocket localSocket;
        localSocket.connectToServer(localServerName);

        if (localSocket.waitForConnected(100)) {
            return 0;
        }
    } else {
        server.removeServer(localServerName);
        server.listen(localServerName);

        QObject::connect(&server, &QLocalServer::newConnection, [&]{
            auto sock = server.nextPendingConnection();
            if (sock) sock->close();
            tokriWindow.onShakeDetect();
        });
    }


    const auto getTrayIcon = [] {
        QPixmap pm(":/tray.png");
        if (ThemeProvider::isTaskbarDark())
            return QIcon(pm);
        QPixmap light(pm.size());
        light.fill(Qt::transparent);
        QPainter p(&light);
        p.drawPixmap(0, 0, pm);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(light.rect(), QColor("#141216"));
        p.end();
        return QIcon(light);
    };

    auto *tray = new QSystemTrayIcon(getTrayIcon(), &a);
    tray->setToolTip("Tokri");
    auto *menu = new QMenu();
    menu->addAction("&Open", &tokriWindow, &TokriWindow::wakeUp);
#ifdef Q_OS_WIN
    menu->addAction("E&xit", &a, &QCoreApplication::quit);
#else
    menu->addAction("&Quit", &a, &QCoreApplication::quit);
#endif
    menu->setPalette(a.palette());
    tray->setContextMenu(menu);

    const auto applyTheme = [menu, tray, getTrayIcon, &tokriWindow] {
        tokriWindow.applyTheme();
        menu->setPalette(tokriWindow.palette());
        tray->setIcon(getTrayIcon());
    };

    QObject::connect(QGuiApplication::styleHints(),
                     &QStyleHints::colorSchemeChanged,
                     &tokriWindow,
                     [applyTheme](Qt::ColorScheme) {
                         applyTheme();
                     });

    QObject::connect(tray, &QSystemTrayIcon::activated,
                     [&](QSystemTrayIcon::ActivationReason r) {
                         if (r == QSystemTrayIcon::DoubleClick)
                             tokriWindow.wakeUp();
                     });

    tray->show();


    auto *listView = tokriWindow.uiHandle()->listView;

    QAction *deleteAction = new QAction(&tokriWindow);
#ifdef Q_OS_MACOS
    deleteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backspace));
#else
    deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
#endif
    tokriWindow.addAction(deleteAction);
    deleteAction->setEnabled(false);

    QAction *copyAction = new QAction(&tokriWindow);
    copyAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Copy));
    tokriWindow.addAction(copyAction);
    copyAction->setEnabled(false);
    QObject::connect(copyAction, &QAction::triggered,
                     &tokriWindow, &TokriWindow::copySelection);

    QAction *openAction = new QAction(&tokriWindow);
#ifdef Q_OS_MACOS
    openAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
#else
    openAction->setShortcuts({ QKeySequence(Qt::Key_Return),
                               QKeySequence(Qt::Key_Enter) });
#endif
    openAction->setShortcutContext(Qt::WidgetShortcut);
    listView->addAction(openAction);
    QObject::connect(openAction, &QAction::triggered,
                     &tokriWindow, &TokriWindow::openSelection);
    openAction->setEnabled(false);

    QAction *renameAction = new QAction(&tokriWindow);
#ifdef Q_OS_MACOS
    renameAction->setShortcut(QKeySequence(Qt::Key_Return));
#else
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
#endif
    renameAction->setShortcutContext(Qt::WidgetShortcut);
    listView->addAction(renameAction);
    QObject::connect(renameAction, &QAction::triggered,
                     &tokriWindow, &TokriWindow::renameSelection);
    renameAction->setEnabled(false);

    // View & Models
    DropAwareFileSystemModel *fsModel = new DropAwareFileSystemModel(&tokriWindow);
    QString rootPath = StandardPaths::getPath(StandardPaths::TokriDir);
    QModelIndex rootIndex = fsModel->setRootPath(rootPath);

    ThumbnailProxyModel *thumbnailProxy = new ThumbnailProxyModel(&tokriWindow);
    thumbnailProxy->setSourceModel(fsModel);
    QObject::connect(QGuiApplication::styleHints(),
                     &QStyleHints::colorSchemeChanged,
                     thumbnailProxy,
                     &ThumbnailProxyModel::clearCache);
    QObject::connect(&tokriWindow,
                     &TokriWindow::themeChanged,
                     thumbnailProxy,
                     &ThumbnailProxyModel::clearCache);

    QAction *pasteAction = new QAction(&tokriWindow);
    pasteAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Paste));
    tokriWindow.addAction(pasteAction);
    auto pasteFromClipboard = [fsModel] {
        auto *clip = QGuiApplication::clipboard();
        if (clip)
            fsModel->dropMimeData(clip->mimeData(), Qt::CopyAction,
                                  -1, -1, QModelIndex());
    };
    QObject::connect(pasteAction, &QAction::triggered, fsModel, pasteFromClipboard);
    QObject::connect(&tokriWindow, &TokriWindow::pasteRequested, fsModel,
                     pasteFromClipboard);

    pasteAction->setEnabled(false);
    const auto updatePasteAction = [pasteAction] {
        auto *clip = QGuiApplication::clipboard();
        pasteAction->setEnabled(
            DropAwareFileSystemModel::isPasteable(clip ? clip->mimeData()
                                                        : nullptr));
    };
    QObject::connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
                     pasteAction, updatePasteAction);
    updatePasteAction();

    FSSortFilterProxy *sortFilterProxy = new FSSortFilterProxy(&tokriWindow);
    sortFilterProxy->setSourceModel(thumbnailProxy);
    sortFilterProxy->setDynamicSortFilter(true);
    sortFilterProxy->sort(0, Qt::DescendingOrder);

    QObject::connect(listView, &NoInternalDragListView::visibleCountChanged,
                     thumbnailProxy, &ThumbnailProxyModel::setVisibleCount);

    tokriWindow.uiHandle()->listView->setModel(sortFilterProxy);
    tokriWindow.uiHandle()->listView->setRootIndex(sortFilterProxy->mapFromSource(
        thumbnailProxy->mapFromSource(rootIndex)));

    const auto updateSelectionActions =
        [copyAction, deleteAction, openAction, renameAction](int count) {
            copyAction->setEnabled(count > 0);
            deleteAction->setEnabled(count > 0);
            openAction->setEnabled(count > 0);
            renameAction->setEnabled(count == 1);
        };

    QObject::connect(listView->selectionModel(),
                     &QItemSelectionModel::selectionChanged,
                     listView,
                     [listView, updateSelectionActions] {
                         updateSelectionActions(
                             listView->selectionModel()->selectedIndexes().size());
                     });

    qRegisterMetaType<QMimeData *>("QMimeData*");

    DropHandler *dropHandler = new DropHandler;
    fsModel->setDropReceiver(dropHandler);
    Logger::instance().log() << "dropReceiver set handler="
                             << static_cast<const void *>(dropHandler);

    QObject::connect(fsModel, &DropAwareFileSystemModel::dropReceived,
                     dropHandler, &DropHandler::handleDrop,
                     Qt::QueuedConnection);

    QObject::connect(dropHandler, &DropHandler::failed,
                     dropHandler,
                     [](const QString &reason) {
                         Logger::instance().log() << "drop failed reason="
                                                  << reason;
                     });


    QThread* th = new QThread;
    th->setObjectName("worker");
    QObject::connect(th, &QThread::finished, dropHandler, &QObject::deleteLater);
    QObject::connect(&a, &QCoreApplication::aboutToQuit, th,
                     [th] {
                         Logger::instance().log() << "aboutToQuit -> quit worker";
                         th->quit();
                     });
    QObject::connect(th, &QThread::finished, th, &QObject::deleteLater);
    dropHandler->moveToThread(th);
    Logger::instance().log() << "worker thread start handlerThread="
                             << static_cast<const void *>(dropHandler->thread());
    th->start();


    // FIXME - move this to dropaware fs model
    DropAwareFileSystemModel::connect(
        deleteAction,
        &QAction::triggered,
        &tokriWindow,
        &TokriWindow::deleteSelection);


    auto SleepShortcut = new QShortcut(QKeySequence("Escape"), &tokriWindow);
    SleepShortcut->setContext(Qt::WindowShortcut);

    QObject::connect(SleepShortcut,
                     &QShortcut::activated,
                     &tokriWindow,
                     &TokriWindow::sleep);

    auto *selectAllShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_A), &tokriWindow);
    selectAllShortcut->setContext(Qt::WindowShortcut);

    QObject::connect(selectAllShortcut,
                     &QShortcut::activated,
                     &tokriWindow,
                     &TokriWindow::selectAll);


#ifdef Q_OS_LINUX
    LinuxDragShakeDetector *interceptor = new LinuxDragShakeDetector;

    QObject::connect(
        interceptor,
        &LinuxDragShakeDetector::shakeDetected,
        &tokriWindow,
        &TokriWindow::onShakeDetect,
        Qt::QueuedConnection
        );
#endif
#ifdef Q_OS_WIN
    WindowsDragShakeDetector *interceptor = new WindowsDragShakeDetector;

    QObject::connect(
        interceptor,
        &WindowsDragShakeDetector::shakeDetected,
        &tokriWindow,
        &TokriWindow::onShakeDetect,
        Qt::QueuedConnection
        );
#endif

#ifdef Q_OS_MACOS
    MacOSDragShakeDetector *interceptor = new MacOSDragShakeDetector;

    QObject::connect(
        interceptor,
        &MacOSDragShakeDetector::shakeDetected,
        &tokriWindow,
        &TokriWindow::onShakeDetect
        );
#endif


    tokriWindow.show();
    applyTheme();
    int ret = a.exec();

#ifdef Q_OS_WIN
    delete interceptor;
#endif

    return ret;
}
