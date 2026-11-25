#include "capture/scrollcapture.h"

#include "capture/screencapture.h"

#include <QPainter>
#include <QTimer>

namespace {
constexpr int kCaptureIntervalMs = 200; // 抓帧间隔
constexpr int kPreviewWidth = 140;      // 侧边栏预览缩略图的宽度
} // namespace

ScrollCapture::ScrollCapture(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &ScrollCapture::captureFrame);
}

void ScrollCapture::start(const QRect& region)
{
    if (m_running)
        return;

    m_region = region;
    m_session.reset();
    m_thumbnail = QImage();
    m_running = true;
    m_timer->start(0);
}

void ScrollCapture::finish()
{
    if (!m_running)
        return;

    m_running = false;
    m_timer->stop();

    if (m_session.isEmpty()) {
        emit canceled();
        return;
    }

    const QImage result = m_session.compose();
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
