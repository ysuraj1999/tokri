#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QWidget>

class QPlainTextEdit;

class LogSink : public QObject
{
    Q_OBJECT

public:
    static LogSink *instance();
    static void install();

    QStringList lines() const;

signals:
    void lineAdded(const QString &line);

private:
    explicit LogSink(QObject *parent = nullptr);
    static void handler(QtMsgType type,
                        const QMessageLogContext &context,
                        const QString &msg);

    mutable QMutex m_mutex;
    QStringList m_lines;
};

class LogWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LogWindow(QWidget *parent = nullptr);

private:
    QPlainTextEdit *m_view;
};

#endif // LOGWINDOW_H
