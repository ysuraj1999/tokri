#include "thumbnailproxymodel.h"
#include "textfile.h"
#include "themeprovider.h"
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

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincodec.h>
#endif

namespace {

constexpr int TotalCacheBytes = 32 * 1024 * 1024;
constexpr int WindowMs = 50;
constexpr int FallbackPendingCapacity = 24;

const QSize ThumbnailSize{ThumbnailLayout::ThumbnailSize,
                          ThumbnailLayout::ThumbnailSize};

#ifdef Q_OS_WIN
QImage loadViaWic(const QString &path)
{
    // Initialize COM for this worker thread if not already initialized
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IWICImagingFactory *pFactory = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFactory)
    );

    if (FAILED(hr) || !pFactory) {
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return {};
    }

    IWICBitmapDecoder *pDecoder = nullptr;
    std::wstring wPath = path.toStdWString();
    hr = pFactory->CreateDecoderFromFilename(
        wPath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &pDecoder
    );

    if (FAILED(hr) || !pDecoder) {
        pFactory->Release();
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return {};
    }

    IWICBitmapFrameDecode *pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr) || !pFrame) {
        pDecoder->Release();
        pFactory->Release();
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return {};
    }

    IWICFormatConverter *pConverter = nullptr;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr) || !pConverter) {
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return {};
    }

    hr = pConverter->Initialize(
        pFrame,
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );

    if (FAILED(hr)) {
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return {};
    }

    UINT width = 0, height = 0;
    pConverter->GetSize(&width, &height);

    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
    if (!result.isNull()) {
        const UINT stride = static_cast<UINT>(result.bytesPerLine());
        const UINT bufferSize = stride * height;
        pConverter->CopyPixels(nullptr, stride, bufferSize, result.bits());
    }

    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pFactory->Release();

    if (SUCCEEDED(hrCo)) {
        CoUninitialize();
    }

    return result;
}
#endif

QImage loadImage(const QString &path, const QSize &target, qreal dpr)
{
    const QSize device = target * dpr;

    QImageReader reader(path);
    reader.setAutoTransform(true);

    const QSize size = reader.size();
    if (size.isValid())
        reader.setScaledSize(size.scaled(device, Qt::KeepAspectRatio));

    QImage image = reader.read();

#ifdef Q_OS_WIN
    if (image.isNull()) {
        // Native Windows WIC decoder fallback for WebP, HEIC, TIFF, etc.
        image = loadViaWic(path);
    }
#endif

    if (image.isNull())
        return image;

    if (!size.isValid() || image.size() != size.scaled(device, Qt::KeepAspectRatio)) {
        image = image.scaled(device, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }

    image.setDevicePixelRatio(dpr);
    return image;
}

QImage loadPreview(const QString &path, qreal dpr, bool isDark)
{
    const QMimeType mime =
        QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent);

    const QString lowerExt = QFileInfo(path).suffix().toLower();

    if (mime.name().startsWith(QLatin1String("image/")) ||
        lowerExt == QLatin1String("webp") ||
        lowerExt == QLatin1String("png") ||
        lowerExt == QLatin1String("jpg") ||
        lowerExt == QLatin1String("jpeg") ||
        lowerExt == QLatin1String("bmp") ||
        lowerExt == QLatin1String("gif")) {
        return loadImage(path, ThumbnailSize, dpr);
    }

    if (TextFile::isTextFile(path, &mime))
        return TextFile::renderThumbnail(path, ThumbnailSize, dpr, isDark);

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

        const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
        const bool isDark = ThemeProvider::isDark();
        watcher->setFuture(QtConcurrent::run(
            [key, dpr, isDark] { return loadPreview(key, dpr, isDark); }));
    }
}

void ThumbnailProxyModel::clearCache()
{
    mCache.clear();
    mFailedRequests.clear();
    mInFlightRequests.clear();
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DecorationRole});
}
