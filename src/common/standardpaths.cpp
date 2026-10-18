#include "common/standardpaths.h"

#include "common/standardnames.h"

#include <QDir>
#include <QStandardPaths>

namespace {

QString ensure(const QString &path)
{
    QDir dir(path);
    if (!dir.exists())
        dir.mkpath(".");
    return dir.absolutePath();
}

}

QString StandardPaths::tokriDir()
{
    return ensure(QStandardPaths::writableLocation(
                      QStandardPaths::HomeLocation)
                  + "/" + StandardNames::get(StandardNames::Directory));
}
