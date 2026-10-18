#include "drop/textfile.h"

#include "common/loghelpers.h"

#include <QTextStream>

TextFile::TextFile(QObject *parent)
    : QObject{parent}
{}

void TextFile::setName(QString name)
{
    mName = name;
}

void TextFile::setContent(QString content)
{
    mContents = content;
}

bool TextFile::save()
{
    Logger &log = Logger::instance();
    log.push("TextFile::save");

    if (mName.isEmpty()) {
        log.log() << "empty name -> false";
        log.pop();
        return false;
    }

    QFile file(mName);
    if (!file.open(QIODevice::NewOnly | QIODevice::WriteOnly | QIODevice::Text)) {
        log.log() << "open failed name=" << mName
                  << "err=" << file.errorString();
        log.pop();
        return false;
    }

    QTextStream out(&file);
    out << mContents;

    if (out.status() != QTextStream::Ok) {
        log.log() << "write failed name=" << mName
                  << "status=" << out.status();
        log.pop();
        return false;
    }

    log.log() << "name=" << mName
              << "chars=" << mContents.size() << "ok=true";

    log.pop();
    return true;
}
