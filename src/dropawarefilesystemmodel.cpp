#include "dropawarefilesystemmodel.h"

#include "loghelpers.h"
#include "textfile.h"

#include <QFileInfo>
#include <QUrl>

namespace {

QMimeData *cloneMimeData(const QMimeData *src)
{
    auto *copy = new QMimeData;
    if (src->hasImage())
        copy->setImageData(src->imageData());
    for (const QString &format : src->formats()) {
        if (copy->hasFormat(format))
            continue;
        copy->setData(format, src->data(format));
    }
    return copy;
}

}

DropAwareFileSystemModel::DropAwareFileSystemModel(QObject *parent)
    : QFileSystemModel{parent}
{
    setReadOnly(false);
}

void DropAwareFileSystemModel::setDropReceiver(QObject *receiver)
{
    mDropReceiver = receiver;
}

Qt::ItemFlags DropAwareFileSystemModel::flags(const QModelIndex &index) const  {
    Qt::ItemFlags f = QFileSystemModel::flags(index);
    return f | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
}

bool DropAwareFileSystemModel::isPasteable(const QMimeData *data)
{
    return data && (data->hasUrls()
                    || data->hasImage()
                    || data->hasText()
                    || data->hasHtml());
}

bool DropAwareFileSystemModel::canDropMimeData(const QMimeData *data,
                                               Qt::DropAction action,
                                               int row, int column,
                                               const QModelIndex &parent) const {
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if (action == Qt::IgnoreAction)
        return true;

    if (!data)
        return false;

    return isPasteable(data);
}

bool DropAwareFileSystemModel::dropMimeData(const QMimeData *data,
                                            Qt::DropAction action,
                                            int row, int column,
                                            const QModelIndex &parent) {
    Logger &log = Logger::instance();
    log.push("dropMimeData");

    log.log() << "action=" << action
              << "row=" << row << "col=" << column
              << "parentValid=" << parent.isValid();
    dumpMimeData(data);

    if (!canDropMimeData(data, action, row, column, parent)) {
        log.log() << "not acceptable -> false";
        log.pop();
        return false;
    }

    QMimeData *copy = cloneMimeData(data);
    log.log() << "cloned formats=" << copy->formats();

    if (mDropReceiver) {
        copy->moveToThread(mDropReceiver->thread());
        log.log() << "receiver=" << static_cast<const void *>(mDropReceiver)
                  << "recvThread="
                  << static_cast<const void *>(mDropReceiver->thread());
    } else {
        log.log() << "no drop receiver, clone leaks";
    }

    log.log() << "emitted dropReceived";
    emit dropReceived(copy);

    log.pop();
    return true;
}

QVariant DropAwareFileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()){
        return QFileSystemModel::data(index, role);
    }

    if (role == Qt::ToolTipRole){
        const QString path = filePath(index);
        const QFileInfo fi = fileInfo(index);
        if (!fi.isFile())
            return fileName(index);

        if (TextFile::isTextFile(path)) {
            auto it = mToolTipCache.find(path);
            if (it != mToolTipCache.end()
                && it->lastModified == fi.lastModified()
                && it->size == fi.size()) {
                return it->text;
            }

            const QString snippet = TextFile::readTextSnippet(path, 1000, 25);
            const QString tip = snippet.trimmed().isEmpty() ? fileName(index) : snippet;
            mToolTipCache.insert(path, {fi.lastModified(), fi.size(), tip});
            return tip;
        }

        return fileName(index);
    }

    return QFileSystemModel::data(index, role);
}

QMimeData* DropAwareFileSystemModel::mimeData(const QModelIndexList &indexes) const
{
    Logger &log = Logger::instance();
    log.push("mimeData");

    QMimeData *mime = QFileSystemModel::mimeData(indexes);
    if (!mime) {
        log.log() << "QFileSystemModel::mimeData returned null";
        log.pop();
        return nullptr;
    }

    const QList<QUrl> urls = mime->urls();
    if (!urls.isEmpty()) {
        // Ensure text/plain is populated for drop targets that don't read text/uri-list
        // (such as text fields, web forms, terminals, and generic drop zones).
        QStringList paths;
        paths.reserve(urls.size());
        for (const QUrl &u : urls) {
            paths.append(u.isLocalFile() ? u.toLocalFile() : u.toString());
        }
        mime->setText(paths.join(QLatin1Char('\n')));

#ifdef Q_OS_LINUX
        // GNOME Nautilus / Files native format: x-special/gnome-copied-files
        // Format: "copy\nfile:///path/to/file1\nfile:///path/to/file2\n"
        QByteArray gnomeData = "copy\n";
        for (const QUrl &u : urls) {
            gnomeData += u.toEncoded() + "\n";
        }
        mime->setData(QStringLiteral("x-special/gnome-copied-files"), gnomeData);

        // Mozilla Firefox drag format: text/x-moz-url
        // Format: URL + '\n' + Title (UTF-16 encoding)
        const QUrl &firstUrl = urls.first();
        const QString title = firstUrl.isLocalFile()
                                  ? QFileInfo(firstUrl.toLocalFile()).fileName()
                                  : firstUrl.toString();
        const QString mozStr = firstUrl.toString() + QLatin1Char('\n') + title;
        QByteArray mozData(reinterpret_cast<const char *>(mozStr.utf16()),
                           mozStr.size() * static_cast<int>(sizeof(char16_t)));
        mime->setData(QStringLiteral("text/x-moz-url"), mozData);
#endif
    }

    log.log() << "drag-out indexes=" << indexes.size()
              << describeMimeData(mime);

    log.pop();
    return mime;
}

Qt::DropActions DropAwareFileSystemModel::supportedDragActions() const {
    Logger &log = Logger::instance();
    log.push("supportedDragActions");

    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction;
    log.log() << "->" << actions;

    log.pop();
    return actions;
}

Qt::DropActions DropAwareFileSystemModel::supportedDropActions() const {
    Logger &log = Logger::instance();
    log.push("supportedDropActions");

    const Qt::DropActions actions = Qt::CopyAction;
    log.log() << "->" << actions;

    log.pop();
    return actions;
}
