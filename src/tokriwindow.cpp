#include "tokriwindow.h"
#include "./ui_tokriwindow.h"
#include "dropawarefilesystemmodel.h"
#include "loghelpers.h"
#include "standardpaths.h"
#include "thumbnaildelegate.h"
#include "thumbnaillayout.h"
#include <QDir>
#include <QMenu>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFileSystemModel>
#include <QFrame>
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TokriWindow::TokriWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TokriWindow)
{
    ui->setupUi(this);

    init();
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

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
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(fileInfoAt(selected[0]).absolutePath()));
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

    hide();

    log.pop();
}

void TokriWindow::wakeUp()
{
    Logger &log = Logger::instance();
    log.push("wakeUp");

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

    const QRect screen = QGuiApplication::screenAt(cursor)->availableGeometry();

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

