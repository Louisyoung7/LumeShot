#include "log/filelogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

#include <cstdio>
#include <cstdlib>

namespace {
constexpr qint64 kMaxLogSize = 1024 * 1024;

struct LoggerState {
    QMutex mutex;
    QString path;
    bool installed = false;
};

LoggerState& state()
{
    static LoggerState instance;
    return instance;
}

QString defaultLogPath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty())
        directory = QDir::homePath() + QStringLiteral("/.local/share/LumeShot");
    return directory + QStringLiteral("/logs/lumeshot.log");
}

QString resolveLogPath()
{
    QString path = qEnvironmentVariable("LUMESHOT_LOG_FILE");
    if (!path.isEmpty())
        path = QFileInfo(path).absoluteFilePath();
    if (path.isEmpty())
        path = defaultLogPath();

    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory)) {
        std::fprintf(stderr, "LumeShot: cannot create log directory: %s\n",
                     qUtf8Printable(directory));
        return QString();
    }
    return path;
}

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "UNKNOWN";
}

void rotateIfNeeded(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists() || info.size() < kMaxLogSize)
        return;

    const QString backup = path + QStringLiteral(".1");
    QFile::remove(backup);
    QFile::rename(path, backup);
}

QString formatMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    QString text = message;
    text.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    text.replace(QLatin1Char('\n'), QStringLiteral("\\n"));

    QString line = QStringLiteral("%1 %2 %3")
                       .arg(QDateTime::currentDateTime().toString(
                                QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")),
                            QString::fromLatin1(levelName(type)), text);
    if (context.file && *context.file) {
        line += QStringLiteral("  [%1:%2]")
                    .arg(QFileInfo(QString::fromUtf8(context.file)).fileName())
                    .arg(context.line);
    }
    return line;
}

void writeToConsole(QtMsgType type, const QString& line)
{
    std::FILE* stream = (type == QtDebugMsg || type == QtInfoMsg) ? stdout : stderr;
    const QByteArray bytes = line.toUtf8();
    std::fprintf(stream, "%s\n", bytes.constData());
    std::fflush(stream);
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString line = formatMessage(type, context, message);
    QMutexLocker locker(&state().mutex);

    writeToConsole(type, line);

    const QString path = state().path;
    if (path.isEmpty())
        return;

    rotateIfNeeded(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        std::fprintf(stderr, "LumeShot: cannot open log file: %s\n", qUtf8Printable(path));
        state().path.clear();
        return;
    }

    QTextStream stream(&file);
    stream << line << QStringLiteral("\n");
    stream.flush();
    file.flush();
}
} // namespace

void FileLogger::install()
{
    QMutexLocker locker(&state().mutex);
    if (state().installed)
        return;

    state().path = resolveLogPath();
    state().installed = true;
    qInstallMessageHandler(messageHandler);
    const QString path = state().path;
    locker.unlock();

    qInfo().noquote() << "LumeShot" << QCoreApplication::applicationVersion()
                      << "started, pid=" << QCoreApplication::applicationPid()
                      << "log=" << (path.isEmpty() ? QStringLiteral("disabled") : path);
}

QString FileLogger::filePath()
{
    QMutexLocker locker(&state().mutex);
    return state().path;
}
