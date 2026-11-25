#include "capture/scrollsession.h"

#include "capture/stitch.h"

#include <QPainter>

ScrollSession::ScrollSession(int maxHeight)
    : m_maxHeight(maxHeight)
{
}

void ScrollSession::reset()
{
    m_tail = QImage();
    m_pieces.clear();
    m_totalHeight = 0;
    m_devicePixelRatio = 1.0;
}

ScrollSession::Result ScrollSession::addFrame(const QImage& frame)
{
    if (frame.isNull())
        return {};

    QImage normalized = frame;
    normalized.setDevicePixelRatio(1.0);

    if (m_pieces.isEmpty()) {
        m_devicePixelRatio = frame.devicePixelRatio();
        m_tail = normalized;
        m_pieces.append(normalized);
        m_totalHeight = normalized.height();
        return {Outcome::Appended, normalized};
    }

    if (normalized.size() != m_tail.size())
        return {Outcome::Ignored, {}};

    const int overlap = stitch::detectOverlap(m_tail, normalized);
    if (overlap < 0)
        return {Outcome::NoScroll, {}};
    if (overlap == 0)
        return {Outcome::NoMatch, {}};

    const int newRows = normalized.height() - overlap;
    if (m_totalHeight + newRows > m_maxHeight)
        return {Outcome::HeightLimit, {}};

    const QImage piece = normalized.copy(0, overlap, normalized.width(), newRows);
    m_pieces.append(piece);
    m_totalHeight += newRows;
    updateTail(normalized, overlap);
    return {Outcome::Appended, piece};
}

void ScrollSession::updateTail(const QImage& frame, int overlap)
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

QImage ScrollSession::compose() const
{
    if (m_pieces.isEmpty())
        return {};

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
