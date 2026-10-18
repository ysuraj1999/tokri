#ifndef FILEPATHPROVIDER_H
#define FILEPATHPROVIDER_H

#include "common/loghelpers.h"
#include "common/standardpaths.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QUuid>

class FilePathProvider : public QObject
{
public:

    explicit FilePathProvider(QObject *parent = nullptr);

    static QString sanitizeName(const QString &name) {
        static const QRegularExpression forbidden(R"([<>:"/\\|?*\x00-\x1F])");
        QString clean = name;
        clean.replace(forbidden, "_");
        clean = clean.trimmed();
        clean.remove(QRegularExpression(R"(^_+|_+$)"));
        bool fallback = false;
        if (clean.isEmpty()) {
            clean = "untitled";
            fallback = true;
        }
        if (clean != name || fallback)
            Logger::instance().log() << "sanitizeName in=" << name
                                     << "out=" << clean << "fallback=" << fallback;
        return clean;
    }

    static QString nameFromPath(const QString &path) {
        const QString rootPath = StandardPaths::tokriDir();
        const QString result = QDir(rootPath).filePath(sanitizeName(QDir(path).dirName()));
        Logger::instance().log() << "nameFromPath path=" << path << "->" << result;
        return result;
    }

    static QString nameWithPrefix(const QString &prefix = QString()) {
        const QString rootPath = StandardPaths::tokriDir();
        QString name = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!prefix.isEmpty())
            name = prefix + "_" + name;
        const QString result = QDir(rootPath).filePath(name);
        Logger::instance().log() << "nameWithPrefix prefix=" << prefix
                                 << "->" << result;
        return result;
    }

    static QString uniquePath(const QString &desiredPath) {
        if (!QFileInfo::exists(desiredPath)) {
            Logger::instance().log() << "uniquePath desired=" << desiredPath
                                     << "chosen=" << desiredPath << "collisions=0";
            return desiredPath;
        }

        const QFileInfo info(desiredPath);
        const QString dir = info.absolutePath();
        const QString base = info.completeBaseName();
        const QString suffix = info.suffix();

        for (int n = 1; ; ++n) {
            QString candidate = base + "_" + QString::number(n);
            if (!suffix.isEmpty())
                candidate += "." + suffix;

            const QString path = QDir(dir).filePath(candidate);
            if (!QFileInfo::exists(path)) {
                Logger::instance().log() << "uniquePath desired=" << desiredPath
                                         << "chosen=" << path << "collisions=" << n;
                return path;
            }
        }
    }
};

#endif // FILEPATHPROVIDER_H
