#include "windows/logwindow.h"

#include <QFont>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPlainTextEdit>
#include <QTime>
#include <QVBoxLayout>
#include <cstdio>

static const int kMaxLines = 2000;

LogSink::LogSink(QObject *parent)
    : QObject(parent)
{
}

LogSink *LogSink::instance()
{
    static LogSink sink;
    return &sink;
}

void LogSink::install()
{
    instance();
    qInstallMessageHandler(&LogSink::handler);
}

QStringList LogSink::lines() const
{
    QMutexLocker locker(&m_mutex);
    return m_lines;
}

void LogSink::handler(QtMsgType type,
                      const QMessageLogContext &context,
                      const QString &msg)
{
    Q_UNUSED(type)
    Q_UNUSED(context)

    const QString line =
        QTime::currentTime().toString("HH:mm:ss.zzz") + QLatin1Char(' ') + msg;

    LogSink *sink = LogSink::instance();
    {
        QMutexLocker locker(&sink->m_mutex);
        sink->m_lines.append(line);
        while (sink->m_lines.size() > kMaxLines)
            sink->m_lines.removeFirst();
    }

    QMetaObject::invokeMethod(
        sink,
        [sink, line] { emit sink->lineAdded(line); },
        Qt::QueuedConnection);
}

LogWindow::LogWindow(QWidget *parent)
    : QWidget(parent)
    , m_view(new QPlainTextEdit(this))
{
    setWindowFlag(Qt::Window);
    setWindowTitle(tr("Logs"));
    resize(600, 400);

    QFont font(QStringLiteral("Monospace"));
    font.setStyleHint(QFont::TypeWriter);

    m_view->setReadOnly(true);
    m_view->setFont(font);
    m_view->setMaximumBlockCount(kMaxLines);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setPlainText(LogSink::instance()->lines().join(QLatin1Char('\n')));

    connect(LogSink::instance(), &LogSink::lineAdded,
            m_view, &QPlainTextEdit::appendPlainText);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
}
