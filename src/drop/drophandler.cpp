#include "drop/drophandler.h"

#include "drop/filepathprovider.h"
#include "common/loghelpers.h"
#include "drop/textfile.h"

#include <QColorSpace>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeDatabase>
#include <QScopedPointer>
#include <QUrl>

namespace {

bool isLinkUrl(const QUrl &url)
{
    return url.isValid() && !url.scheme().isEmpty();
}

}

DropHandler::DropHandler(QObject *parent)
    : QObject{parent}
{}

void DropHandler::handleDrop(QMimeData *data)
{
    Logger &log = Logger::instance();
    log.push("handleDrop");

    if (!data) {
        log.log() << "null data";
        log.pop();
        return;
    }

    log.log() << "urls=" << data->urls().size();

    QScopedPointer<QMimeData> guard(data);
    dumpMimeData(data);

    const char *consumer = "none";
    bool wrote = false;

    if (tryMultiUrls(data)) {
        consumer = "multi-url";
        wrote = true;
    } else if (tryLocalFile(data)) {
        consumer = "local";
        wrote = true;
    } else if (tryImage(data)) {
        consumer = "image";
        wrote = true;
    } else if (tryRemoteLink(data)) {
        consumer = "remote-link";
        wrote = true;
    } else if (tryText(data)) {
        consumer = "text";
        wrote = true;
    } else if (tryHtml(data)) {
        consumer = "html";
        wrote = true;
    }

    log.log() << "consumer=" << consumer << "wrote=" << wrote;

    if (wrote) {
        log.log() << "emit changed";
        emit changed();
    } else {
        log.log() << "stored nothing";
    }

    log.pop();
}

bool DropHandler::tryMultiUrls(const QMimeData *data)
{
    Logger &log = Logger::instance();
    log.push("multi-url");

    const QList<QUrl> urls = data->urls();
    if (urls.size() <= 1) {
        log.pop();
        return false;
    }

    log.log() << "count=" << urls.size();

    bool wrote = false;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile() && !isLinkUrl(url)) {
            log.log() << "skipping non-link url entry=" << url.toString();
            continue;
        }
        wrote |= storeUrlItem(url);
    }

    log.pop();
    return wrote;
}

bool DropHandler::tryLocalFile(const QMimeData *data)
{
    Logger &log = Logger::instance();
    log.push("local");

    const QList<QUrl> urls = data->urls();
    if (urls.size() != 1) {
        log.pop();
        return false;
    }

    const QUrl url = urls.first();
    if (!url.isLocalFile()) {
        log.pop();
        return false;
    }

    log.log() << "url=" << url.toString();
    const bool wrote = storeUrlItem(url);

    log.pop();
    return wrote;
}

bool DropHandler::tryImage(const QMimeData *data)
{
    Logger &log = Logger::instance();
    log.push("image");
    const bool wrote = saveFirstImage(data);
    log.pop();
    return wrote;
}

bool DropHandler::tryRemoteLink(const QMimeData *data)
{
    Logger &log = Logger::instance();
    log.push("remote-link");

    const QList<QUrl> urls = data->urls();
    if (urls.size() != 1) {
        log.pop();
        return false;
    }

    const QUrl url = urls.first();
    if (url.isLocalFile()) {
        log.pop();
        return false;
    }
    if (!isLinkUrl(url)) {
        log.log() << "skipping non-link url entry=" << url.toString();
        log.pop();
        return false;
    }

    log.log() << "url=" << url.toString();
    const bool wrote = storeUrlItem(url);

    log.pop();
    return wrote;
}

bool DropHandler::tryText(const QMimeData *data)
{
    if (!data->hasText())
        return false;

    Logger &log = Logger::instance();
    log.push("text");
    const QString text = data->text();
    log.log() << "chars=" << text.size();
    const bool wrote = saveText(text);
    log.pop();
    return wrote;
}

bool DropHandler::tryHtml(const QMimeData *data)
{
    if (!data->hasHtml())
        return false;

    Logger &log = Logger::instance();
    log.push("html");
    const QString html = data->html();
    log.log() << "chars=" << html.size();
    const bool wrote = saveHtml(html);
    log.pop();
    return wrote;
}

bool DropHandler::storeUrlItem(const QUrl &url)
{
    Logger &log = Logger::instance();
    log.push("url-item");

    if (url.isLocalFile()) {
        const QFileInfo info(url.toLocalFile());
        log.log() << "local url=" << url.toString()
                  << "isDir=" << info.isDir()
                  << "isFile=" << info.isFile();

        bool wrote = false;
        if (info.isDir())
            wrote = copyDirectory(info.absoluteFilePath());
        else if (info.isFile())
            wrote = copyFile(info.absoluteFilePath());
        else
            log.log() << "url is local but neither file nor dir, skipped";

        log.pop();
        return wrote;
    }

    log.log() << "link url=" << url.toString();
    const bool wrote = saveUrl(url.toString());
    log.pop();
    return wrote;
}

bool DropHandler::saveFirstImage(const QMimeData *data)
{
    Logger &log = Logger::instance();

    for (const QString &format : data->formats()) {
        if (!format.startsWith("image/"))
            continue;
        if (!QImageReader::supportedMimeTypes().contains(format.toLatin1())) {
            log.log() << "skipping unsupported image format=" << format;
            continue;
        }

        const QByteArray bytes = data->data(format);
        if (bytes.isEmpty()) {
            log.log() << "empty image bytes for format=" << format;
            continue;
        }

        log.log() << "image-bytes mime=" << format << "bytes=" << bytes.size();
        if (saveImageBytes(bytes, format))
            return true;
    }

    if (data->hasImage()) {
        const QImage image = data->imageData().value<QImage>();
        log.log() << "image-data null=" << image.isNull()
                  << "size=" << image.width() << "x" << image.height();
        if (!image.isNull())
            return saveImage(image);
    }

    return false;
}

bool DropHandler::copyFile(const QString &filePath)
{
    const QString dst = FilePathProvider::uniquePath(
        FilePathProvider::nameFromPath(filePath));

    bool ok = false;

#if defined(Q_OS_MACOS)
    QFile in(filePath);
    QFile out(dst);

    if (!in.open(QIODevice::ReadOnly) ||
        !out.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        out.write(in.readAll()) == -1) {
        emit failed(filePath);
        ok = false;
    } else {
        ok = true;
    }
#else
    if (!QFile::copy(filePath, dst)) {
        emit failed(filePath);
        ok = false;
    } else {
        ok = true;
    }
#endif

    Logger::instance().log() << "copyFile src=" << filePath
                             << "dst=" << dst << "ok=" << ok;
    return ok;
}

bool DropHandler::copyDirectory(const QString &directory)
{
    const QString dstFinal = FilePathProvider::uniquePath(
        FilePathProvider::nameFromPath(directory));

#if defined(Q_OS_MACOS)
    const QString dst = dstFinal + ".progress";
#else
    const QString dst = dstFinal;
#endif

    Logger &log = Logger::instance();
    log.log() << "copyDirectory src=" << directory
              << "dst=" << dst << "dstFinal=" << dstFinal;

    if (!QDir().mkpath(dst)) {
        log.log() << "copyDirectory mkpath failed dst=" << dst;
        emit failed(dst);
        return false;
    }

    int files = 0;
    QDirIterator it(directory,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();

        const QString rel = QDir(directory).relativeFilePath(it.filePath());
        const QString out = dst + "/" + rel;

        if (it.fileInfo().isDir()) {
            if (!QDir().mkpath(out)) {
                log.log() << "copyDirectory subdir mkpath failed out=" << out;
                emit failed(out);
                return false;
            }
        } else {
            QDir().mkpath(QFileInfo(out).path());
            if (!QFile::copy(it.filePath(), out)) {
                log.log() << "copyDirectory file copy failed src=" << it.filePath()
                          << "out=" << out;
                emit failed(out);
                return false;
            }
            ++files;
        }
    }

#if defined(Q_OS_MACOS)
    if (QFileInfo::exists(dstFinal))
        QDir(dstFinal).removeRecursively();

    QDir tmpDir = QFileInfo(dstFinal).dir();
    if (!tmpDir.rename(QFileInfo(dst).fileName(), QFileInfo(dstFinal).fileName())) {
        log.log() << "copyDirectory final rename failed"
                  << "from=" << dst << "to=" << dstFinal;
    }
#endif

    log.log() << "copyDirectory ok files=" << files
              << "dstFinal=" << dstFinal;
    return true;
}

bool DropHandler::saveImageBytes(const QByteArray &bytes, const QString &mimeType)
{
    QMimeDatabase db;
    QString suffix = db.mimeTypeForName(mimeType).preferredSuffix();
    if (suffix.isEmpty())
        suffix = "png";

    const QString path = FilePathProvider::uniquePath(
        FilePathProvider::nameWithPrefix("image") + "." + suffix);

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        Logger::instance().log() << "saveImageBytes open failed path=" << path
                                 << "err=" << out.errorString();
        emit failed(path);
        return false;
    }

    if (out.write(bytes) == -1) {
        Logger::instance().log() << "saveImageBytes write failed path=" << path
                                 << "err=" << out.errorString();
        emit failed(path);
        return false;
    }

    Logger::instance().log() << "saveImageBytes mime=" << mimeType
                             << "suffix=" << suffix << "bytes=" << bytes.size()
                             << "path=" << path << "ok=true";
    return true;
}

bool DropHandler::saveImage(const QImage &image)
{
    const QString path = FilePathProvider::uniquePath(
        FilePathProvider::nameWithPrefix("image") + ".png");

    QImage out = image;

#ifdef Q_OS_MACOS
    out.setColorSpace(QColorSpace());
    out = out.convertToFormat(QImage::Format_ARGB32);
#endif

    QImageWriter writer(path, "png");
    if (!writer.write(out)) {
        Logger::instance().log() << "saveImage write failed path=" << path
                                 << "err=" << writer.errorString();
        emit failed(path);
        return false;
    }

    Logger::instance().log() << "saveImage size=" << out.width() << "x" << out.height()
                             << "path=" << path << "ok=true";
    return true;
}

bool DropHandler::saveUrl(const QString &url)
{
    TextFile file;
    file.setName(FilePathProvider::nameWithPrefix("link") + ".txt");
    file.setContent(url);
    const bool ok = file.save();
    Logger::instance().log() << "saveUrl chars=" << url.size()
                             << "preview=" << preview(url) << "ok=" << ok;
    if (!ok)
        emit failed(url);
    return ok;
}

bool DropHandler::saveText(const QString &text)
{
    TextFile file;
    file.setName(FilePathProvider::nameWithPrefix("text") + ".txt");
    file.setContent(text);
    const bool ok = file.save();
    Logger::instance().log() << "saveText chars=" << text.size()
                             << "preview=" << preview(text) << "ok=" << ok;
    if (!ok)
        emit failed(text);
    return ok;
}

bool DropHandler::saveHtml(const QString &html)
{
    TextFile file;
    file.setName(FilePathProvider::nameWithPrefix("text") + ".html");
    file.setContent(html);
    const bool ok = file.save();
    Logger::instance().log() << "saveHtml chars=" << html.size()
                             << "preview=" << preview(html) << "ok=" << ok;
    if (!ok)
        emit failed(html);
    return ok;
}
