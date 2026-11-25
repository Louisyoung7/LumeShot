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
    m_previousFrame = QImage();
    m_pieces.clear();
    m_totalHeight = 0;
    m_misses = 0;
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
        m_previousFrame = normalized;
        m_pieces.append(normalized);
        m_totalHeight = normalized.height();
        return {Outcome::Appended, normalized};
    }

    if (normalized.size() != m_tail.size())
        return {Outcome::Ignored, {}};

    // 当前帧与上一帧基本一致时，认为用户没有滚动。
    // 这样可以忽略光标闪烁和小面积动画，避免静止画面被反复追加。
    if (!m_previousFrame.isNull() && stitch::framesMostlySame(m_previousFrame, normalized)) {
        m_previousFrame = normalized;
        m_misses = 0;
        return {Outcome::NoScroll, {}, -1};
    }
    m_previousFrame = normalized;

    const int overlap = stitch::detectOverlap(m_tail, normalized);
    if (overlap < 0) {
        m_misses = 0;
        return {Outcome::NoScroll, {}, -1};
    }
    if (overlap == 0) {
        ++m_misses;

        // 连续多帧无法重叠时重新锚点，避免滚动过快或短暂异常后彻底停止。
        // 低纹理画面不重新锚点，保留空白压缩的行为。
        if (m_misses < kMaxConsecutiveMisses || !stitch::hasReliableTexture(normalized))
            return {Outcome::NoMatch, {}};

        const Result result = appendWholeFrame(normalized);
        if (result.outcome == Outcome::Appended)
            m_misses = 0;
        return result;
    }

    m_misses = 0;
    const int newRows = normalized.height() - overlap;
    if (m_totalHeight + newRows > m_maxHeight)
        return {Outcome::HeightLimit, {}};

    const QImage piece = normalized.copy(0, overlap, normalized.width(), newRows);

    // 页面到底后，部分应用会继续回弹，露出的是背景而不是新内容。
    // 新增部分纵向几乎没有变化时，视为没有真正滚动，避免每滚一下追加一条空白。
    if (!stitch::hasVisibleContent(piece))
        return {Outcome::NoScroll, {}, -1};

    m_pieces.append(piece);
    m_totalHeight += newRows;
    updateTail(normalized, overlap);
    return {Outcome::Appended, piece, overlap};
}

// 重新锚点：当前帧无法与旧内容重叠，整体作为新内容追加。
// 接缝处可能重复或跳过少量内容，但比彻底停止拼接更可用。
ScrollSession::Result ScrollSession::appendWholeFrame(const QImage& frame)
{
    if (m_totalHeight + frame.height() > m_maxHeight)
        return {Outcome::HeightLimit, {}};

    m_pieces.append(frame);
    m_totalHeight += frame.height();
    m_tail = frame;
    return {Outcome::Appended, frame, 0};
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
