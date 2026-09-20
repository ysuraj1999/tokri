#include "dropawarefilesystemmodel.h"

#include "loghelpers.h"

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
        return fileName(index);
    }

    return QFileSystemModel::data(index, role);
}

QMimeData* DropAwareFileSystemModel::mimeData(const QModelIndexList &indexes) const
{
    Logger &log = Logger::instance();
    log.push("mimeData");

    QMimeData *mime = QFileSystemModel::mimeData(indexes);

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
