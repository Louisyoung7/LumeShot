#include "capture/scrollcapture.h"

#include "capture/screencapture.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QTimer>

namespace {
constexpr int kCaptureIntervalMs = 200; // 抓帧间隔
constexpr int kPreviewWidth = 140;      // 侧边栏预览缩略图的宽度

const char* outcomeName(ScrollSession::Outcome outcome)
{
    switch (outcome) {
    case ScrollSession::Outcome::Ignored:
        return "ignored";
    case ScrollSession::Outcome::NoScroll:
        return "no-scroll";
    case ScrollSession::Outcome::NoMatch:
        return "no-match";
    case ScrollSession::Outcome::HeightLimit:
        return "height-limit";
    case ScrollSession::Outcome::Appended:
        return "appended";
    }
    return "unknown";
}
} // namespace

ScrollCapture::ScrollCapture(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &ScrollCapture::captureFrame);
}

void ScrollCapture::start(const QRect& region, int firstDelayMs)
{
    if (m_running)
        return;

    m_region = region;
    m_session.reset();
    m_thumbnail = QImage();
    m_debugIndex = 0;
    openDebugDump();
    m_running = true;
    m_timer->start(firstDelayMs);
}

void ScrollCapture::finish()
{
    if (!m_running)
        return;

    m_running = false;
    m_timer->stop();

    if (m_session.isEmpty()) {
        closeDebugDump(QImage(), QStringLiteral("empty"));
        emit canceled();
        return;
    }

    const QImage result = m_session.compose();
    closeDebugDump(result, QStringLiteral("finished"));
    m_session.reset();
    m_thumbnail = QImage();
    emit finished(result);
}

void ScrollCapture::cancel()
{
    if (!m_running)
        return;

    m_running = false;
    m_timer->stop();
    closeDebugDump(QImage(), QStringLiteral("canceled"));
    m_session.reset();
    m_thumbnail = QImage();
    emit canceled();
}

void ScrollCapture::scheduleNext()
{
    if (m_running)
        m_timer->start(kCaptureIntervalMs);
}

void ScrollCapture::captureFrame()
{
    if (!m_running)
        return;

    const QPixmap desktop = ScreenCapture::grabDesktop();
    QImage frame = ScreenCapture::crop(desktop, m_region).toImage();
    if (frame.isNull()) {
        scheduleNext();
        return;
    }
    frame = frame.convertToFormat(QImage::Format_RGB32);

    const ScrollSession::Result result = m_session.addFrame(frame);
    if (!m_debugDir.isEmpty()) {
        dumpFrame(frame);
        const QString line =
            QStringLiteral("frame=%1 outcome=%2 overlap=%3 misses=%4 size=%5x%6 total=%7")
                .arg(m_debugIndex)
                .arg(QLatin1String(outcomeName(result.outcome)))
                .arg(result.overlap)
                .arg(m_session.consecutiveMisses())
                .arg(frame.width())
                .arg(frame.height())
                .arg(m_session.totalHeight());
        writeDebugLine(line);
        qInfo().noquote() << "scroll" << line;
    }
    if (result.outcome == ScrollSession::Outcome::Appended)
        appendThumbnail(result.piece);

    emit previewChanged(m_thumbnail,
                        qRound(m_session.totalHeight() / m_session.devicePixelRatio()));
    scheduleNext();
}

// 缩略图按固定宽度纵向累加，避免每次更新都重建整张长图
void ScrollCapture::appendThumbnail(const QImage& piece)
{
    if (piece.isNull() || piece.height() <= 0)
        return;

    const int width = kPreviewWidth;
    const int height = qMax(1, qRound(qreal(piece.height()) * width / piece.width()));
    const QImage scaled =
        piece.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    if (m_thumbnail.isNull()) {
        m_thumbnail = scaled;
        return;
    }

    QImage combined(width, m_thumbnail.height() + scaled.height(), QImage::Format_RGB32);
    QPainter painter(&combined);
    painter.drawImage(0, 0, m_thumbnail);
    painter.drawImage(0, m_thumbnail.height(), scaled);
    painter.end();
    m_thumbnail = combined;
}

// 开启方式：LUMESHOT_SCROLL_DEBUG=1（导出到临时目录）或 LUMESHOT_SCROLL_DEBUG=/某个目录。
// 每次会话单独建一个带时间戳的子目录，避免覆盖上一次的记录。
void ScrollCapture::openDebugDump()
{
    if (m_debugLog) {
        m_debugLog->close();
        delete m_debugLog;
        m_debugLog = nullptr;
    }
    m_debugDir.clear();

    const QString value = qEnvironmentVariable("LUMESHOT_SCROLL_DEBUG");
    if (value.isEmpty())
        return;

    const QString base = value == QLatin1String("1")
        ? QDir::tempPath() + QStringLiteral("/lumeshot-scroll")
        : value;

    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    QString dir = base + QLatin1Char('/') + stamp;
    for (int suffix = 2; QDir(dir).exists(); ++suffix) {
        dir = base + QLatin1Char('/') + stamp + QLatin1Char('-') + QString::number(suffix);
    }

    if (!QDir().mkpath(dir + QStringLiteral("/frames")))
        return;

    m_debugLog = new QFile(dir + QStringLiteral("/scroll.log"), this);
    if (!m_debugLog->open(QIODevice::WriteOnly | QIODevice::Text)) {
        delete m_debugLog;
        m_debugLog = nullptr;
        return;
    }

    m_debugDir = dir;
    qInfo().noquote() << "scroll debug dump:" << m_debugDir;
}

void ScrollCapture::dumpFrame(const QImage& frame)
{
    ++m_debugIndex;
    const QString name =
        QStringLiteral("frame_%1.png").arg(m_debugIndex, 4, 10, QLatin1Char('0'));
    frame.save(m_debugDir + QStringLiteral("/frames/") + name, "PNG");
}

void ScrollCapture::writeDebugLine(const QString& line)
{
    if (!m_debugLog)
        return;

    m_debugLog->write(line.toUtf8());
    m_debugLog->write("\n");
    m_debugLog->flush();
}

void ScrollCapture::closeDebugDump(const QImage& result, const QString& status)
{
    if (m_debugDir.isEmpty())
        return;

    if (!result.isNull())
        result.save(m_debugDir + QStringLiteral("/result.png"), "PNG");

    writeDebugLine(QStringLiteral("status=%1 frames=%2 total=%3")
                       .arg(status)
                       .arg(m_debugIndex)
                       .arg(m_session.totalHeight()));

    if (m_debugLog) {
        m_debugLog->close();
        delete m_debugLog;
        m_debugLog = nullptr;
    }
    m_debugDir.clear();
}
