#include "filereveal.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#if defined(Q_OS_LINUX) && defined(TOKRI_HAVE_DBUS)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#endif

namespace FileReveal {

void revealInFileManager(const QString &path)
{
    const QFileInfo fi(path);
#ifdef Q_OS_MACOS
    QProcess::startDetached("/usr/bin/open", { "-R", path });
#elif defined(Q_OS_WIN)
    QProcess::startDetached("explorer.exe",
        fi.isDir() ? QStringList{ QDir::toNativeSeparators(path) }
                   : QStringList{ "/select,", QDir::toNativeSeparators(path) });
#elif defined(Q_OS_LINUX) && defined(TOKRI_HAVE_DBUS)
    QDBusInterface fm("org.freedesktop.FileManager1",
                      "/org/freedesktop/FileManager1",
                      "org.freedesktop.FileManager1",
                      QDBusConnection::sessionBus());
    if (fm.isValid()
        && fm.call("ShowItems",
                   QStringList{ QUrl::fromLocalFile(path).toString() },
                   QString()).type() != QDBusMessage::ErrorMessage)
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
}

} // namespace FileReveal
