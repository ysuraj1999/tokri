#ifndef LOGHELPERS_H
#define LOGHELPERS_H

#include <QDebug>
#include <QImage>
#include <QMimeData>
#include <QStringList>
#include <QThread>
#include <QUrl>

class Logger
{
public:
    static Logger &instance()
    {
        static thread_local Logger inst;
        return inst;
    }

    void push(const QString &tag)
    {
        if (!tag.isEmpty())
            m_tags.append(tag);
    }

    void pop()
    {
        if (!m_tags.isEmpty())
            m_tags.removeLast();
    }

    QDebug log() const
    {
        QDebug out = qInfo().noquote();
        out << QStringLiteral("[%1]").arg(tags().join(QLatin1Char(':')));
        return out;
    }

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

private:
    Logger() = default;

    QStringList tags() const
    {
        const QString name = QThread::currentThread()->objectName();
        QStringList all;
        all << (name.isEmpty() ? QStringLiteral("?") : name);
        all += m_tags;
        return all;
    }

    QStringList m_tags;
};

inline QString preview(const QString &s, int max = 120)
{
    QString one = s;
    one.replace('\n', "\\n");
    one.replace('\r', "\\r");
    one.replace('\t', "\\t");
    if (one.size() > max)
        one = one.left(max) + QStringLiteral("...(+%1 more)").arg(s.size() - max);
    return one;
}

inline QString describeMimeData(const QMimeData *md)
{
    if (!md)
        return QStringLiteral("<null mimeData>");

    const QStringList fmts = md->formats();

    return QStringLiteral("hasUrls=%1 hasImage=%2 hasText=%3 hasHtml=%4 formats=[%5]")
        .arg(md->hasUrls()).arg(md->hasImage())
        .arg(md->hasText()).arg(md->hasHtml())
        .arg(fmts.join(", "));
}

inline void dumpMimeData(const QMimeData *md)
{
    Logger::instance().log() << describeMimeData(md);
    if (!md)
        return;

    for (const QUrl &u : md->urls())
        Logger::instance().log() << "url:" << u.toString()
                                 << "local=" << u.isLocalFile();

    if (md->hasText())
        Logger::instance().log() << "text:" << preview(md->text());
    if (md->hasHtml())
        Logger::instance().log() << "html:" << preview(md->html());

    if (md->hasImage()) {
        const QImage img = md->imageData().value<QImage>();
        Logger::instance().log() << "image:"
            << (img.isNull() ? QStringLiteral("<null>")
                             : QStringLiteral("%1x%2").arg(img.width()).arg(img.height()));
    }
}

#endif // LOGHELPERS_H
