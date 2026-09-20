#include "thumbnailproxymodel.h"

#include "thumbnaillayout.h"

#include <QApplication>
#include <QFileSystemModel>
#include <QFutureWatcher>
#include <QIcon>
#include <QImageReader>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QThread>
#include <QtConcurrent>

namespace {

constexpr int TotalCacheBytes = 32 * 1024 * 1024;
constexpr int WindowMs = 50;
constexpr int FallbackPendingCapacity = 24;

const QSize ThumbnailSize{ThumbnailLayout::ThumbnailSize,
                          ThumbnailLayout::ThumbnailSize};

QImage loadImage(const QString &path, const QSize &target, qreal dpr)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);

    const QSize device = target * dpr;
    const QSize size = reader.size();
    if (size.isValid())
        reader.setScaledSize(size.scaled(device, Qt::KeepAspectRatio));

    QImage image = reader.read();
    if (image.isNull())
        return image;

    if (!size.isValid())
        image = image.scaled(device, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);

    image.setDevicePixelRatio(dpr);
    return image;
}

QImage loadPreview(const QString &path)
{
    const QMimeType mime =
        QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent);

    if (mime.name().startsWith(QLatin1String("image/")))
        return loadImage(path, ThumbnailSize, qApp->devicePixelRatio());

    return {};
}

QString pathOf(const QPersistentModelIndex &pidx)
{
    return pidx.data(QFileSystemModel::FileInfoRole)
        .value<QFileInfo>()
        .absoluteFilePath();
}

}

ThumbnailProxyModel::ThumbnailProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , mVisibleCount(FallbackPendingCapacity)
{
    mCache.setMaxCost(TotalCacheBytes);

    mDebounceTimer.setSingleShot(true);
    mDebounceTimer.setInterval(WindowMs);
    connect(&mDebounceTimer, &QTimer::timeout, this,
            &ThumbnailProxyModel::dispatchPendingRequests);
}

bool ThumbnailProxyModel::canDropMimeData(const QMimeData *data,
                                          Qt::DropAction action,
                                          int row, int column,
                                          const QModelIndex &parent) const
{
    QAbstractItemModel *src = sourceModel();
    if (!src)
        return false;
    return src->canDropMimeData(data, action, row, column,
                                mapToSource(parent));
}

QVariant ThumbnailProxyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DecorationRole)
        return QSortFilterProxyModel::data(index, role);

    const QFileInfo fi =
        index.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();
    if (!fi.isFile())
        return QSortFilterProxyModel::data(index, role);

    const QString key = fi.absoluteFilePath();

    if (const QPixmap *pm = mCache.object(key))
        return QIcon(*pm);

    if (mFailedRequests.contains(key) || mInFlightRequests.contains(key)
        || mPendingRequests.contains(QPersistentModelIndex(index)))
        return QSortFilterProxyModel::data(index, role);

    requestThumbnail(index);

    return QSortFilterProxyModel::data(index, role);
}

void ThumbnailProxyModel::requestThumbnail(const QModelIndex &index) const
{
    mPendingRequests.append(QPersistentModelIndex(index));
    mDebounceTimer.start();
}

void ThumbnailProxyModel::setVisibleCount(int count)
{
    mVisibleCount = count;
    mDebounceTimer.start();
}

void ThumbnailProxyModel::dispatchPendingRequests()
{
    while (mPendingRequests.size() > mVisibleCount)
        mPendingRequests.removeFirst();

    const int limit = QThread::idealThreadCount();

    while (!mPendingRequests.isEmpty() && mInFlightRequests.size() < limit) {
        const QPersistentModelIndex pidx = mPendingRequests.takeLast();
        if (!pidx.isValid())
            continue;

        const QString key = pathOf(pidx);
        mInFlightRequests.insert(key);

        auto *watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this,
                [this, watcher, key, pidx] {
                    const QImage img = watcher->result();
                    watcher->deleteLater();

                    mInFlightRequests.remove(key);

                    if (img.isNull()) {
                        mFailedRequests.insert(key);
                    } else {
                        mCache.insert(key, new QPixmap(QPixmap::fromImage(img)),
                                      int(img.sizeInBytes()));
                        if (pidx.isValid())
                            emit dataChanged(pidx, pidx,
                                             {Qt::DecorationRole});
                    }

                    if (!mPendingRequests.isEmpty() && !mDebounceTimer.isActive())
                        dispatchPendingRequests();
                });

        watcher->setFuture(QtConcurrent::run(
            [key] { return loadPreview(key); }));
    }
}
