#include "capture/scrollcapture.h"

#include "capture/screencapture.h"
#include "capture/stitch.h"

#include <QPainter>
#include <QTimer>

namespace {
constexpr int kCaptureIntervalMs = 200;       // 抓帧间隔
constexpr int kMaxPixelsHeight = 20000;       // 拼接高度上限（物理像素）
constexpr int kPreviewWidth = 140;            // 侧边栏预览缩略图的宽度
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
    m_tail = QImage();
    m_thumbnail = QImage();
    m_pieces.clear();
    m_totalHeight = 0;
    m_devicePixelRatio = 1.0;
    m_running = true;
    m_timer->start(0);
}

void ScrollCapture::finish()
{
    if (!m_running)
        return;

    m_running = false;
    m_timer->stop();

    if (m_pieces.isEmpty()) {
        emit canceled();
        return;
    }

    const QImage result = compose();
    m_pieces.clear();
    m_tail = QImage();
    m_thumbnail = QImage();
    emit finished(result);
}

void ScrollCapture::cancel()
{
    if (!m_running)
        return;

    m_running = false;
    m_timer->stop();
    m_pieces.clear();
    m_tail = QImage();
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

    if (m_pieces.isEmpty()) {
        m_devicePixelRatio = frame.devicePixelRatio();
        frame.setDevicePixelRatio(1.0);
        m_tail = frame;
        m_pieces.append(frame);
        appendThumbnail(frame);
        m_totalHeight = frame.height();
    } else if (frame.size() == m_tail.size()) {
        frame.setDevicePixelRatio(1.0);
        const int overlap = stitch::detectOverlap(m_tail, frame);
        if (overlap > 0 && m_totalHeight + (frame.height() - overlap) <= kMaxPixelsHeight) {
            const int newRows = frame.height() - overlap;
            const QImage piece = frame.copy(0, overlap, frame.width(), newRows);
            m_pieces.append(piece);
            appendThumbnail(piece);
            m_totalHeight += newRows;
            updateTail(frame, overlap);
        }
        // overlap 为 0：这一帧匹配不可靠，直接丢弃，避免拼出重复内容
    }

    emit previewChanged(m_thumbnail, qRound(m_totalHeight / m_devicePixelRatio));
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

void ScrollCapture::updateTail(const QImage& frame, int overlap)
{
    const int height = frame.height();
    const int newRows = height - overlap;
    if (newRows <= 0)
        return;

    QImage combined(frame.width(), height + newRows, QImage::Format_RGB32);
    QPainter painter(&combined);
    painter.drawImage(0, 0, m_tail);
    painter.drawImage(0, height, frame, 0, overlap, frame.width(), newRows);
    painter.end();

    // 只保留底部最多一屏，供下一帧比对
    m_tail = combined.copy(0, newRows, combined.width(), height);
}

QImage ScrollCapture::compose() const
{
    const QImage& first = m_pieces.first();
    QImage result(first.width(), m_totalHeight, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::black);

    QPainter painter(&result);
    int y = 0;
    for (const QImage& piece : m_pieces) {
        painter.drawImage(QPoint(0, y), piece);
        y += piece.height();
    }
    result.setDevicePixelRatio(m_devicePixelRatio);
    return result;
}
