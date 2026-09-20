#include "sortfilterproxy.h"
// #include <rapidfuzz/fuzz.hpp>

#include <QFileSystemModel>
#include <QMimeData>

FSSortFilterProxy::FSSortFilterProxy(QObject *parent)
    : QSortFilterProxyModel{parent}
{}

bool FSSortFilterProxy::canDropMimeData(const QMimeData *data,
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

bool FSSortFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto leftFileInfo = left.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();
    const auto rightFileInfo = right.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();
    return leftFileInfo.birthTime() < rightFileInfo.birthTime();

    // if (mSearch.isEmpty()){
    // }

    // return score(leftFileInfo.fileName()) < score(rightFileInfo.fileName());
}

bool FSSortFilterProxy::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    if (mSearch.isEmpty())
        return true;

    auto *src = sourceModel();
    if (!src)
        return false;

    const QModelIndex idx = src->index(row, 0, parent);
    if (!idx.isValid())
        return false;

    const QFileInfo info =
        idx.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();

    if (!info.exists())
        return false;

    if (info.isDir())
        return true;

    const QString name = info.fileName();
    return true;
    // return score(name) > 30;
}

// void FSSortFilterProxy::setSearch(const QString &string)
// {
//     if (mSearch != string){
//         beginFilterChange();
//         mSearch = string;
//         endFilterChange();
//     }
// }

// double FSSortFilterProxy::score(const QString &candidate) const
// {
//     const auto result = rapidfuzz::fuzz::ratio(
//         mSearch.toStdString(),
//         candidate.toLower().toStdString());
//     return result;
// }
