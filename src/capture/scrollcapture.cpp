#include "capture/scrollcapture.h"

#include "capture/screencapture.h"

#include <QPainter>
#include <QTimer>

#include <algorithm>

namespace {
constexpr int kSignatureWidth = 48;           // 行特征宽度，越小比对越快
constexpr int kCaptureIntervalMs = 200;       // 抓帧间隔
constexpr int kMaxPixelsHeight = 20000;       // 拼接高度上限（物理像素）
constexpr int kMinOverlapRows = 16;           // 最少重叠行数
constexpr double kSameFrameDifference = 1.2;  // 低于该值视为画面没动
constexpr double kCoarseDifference = 6.0;     // 粗筛阈值（行特征平均灰度差）
constexpr double kVerifiedDifference = 5.0;   // 精验阈值（原图采样平均灰度差）
constexpr int kPreviewWidth = 140;            // 侧边栏预览缩略图的宽度

// 把画面缩成窄条，每行就是一段灰度特征
QImage rowSignature(const QImage& image)
{
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    return gray.scaled(kSignatureWidth, gray.height(), Qt::IgnoreAspectRatio,
                       Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_Grayscale8);
}

// 比较两张特征图指定行的平均灰度差
double rowDifference(const QImage& a, int aRow, const QImage& b, int bRow, int rows)
{
    double total = 0;
    for (int y = 0; y < rows; ++y) {
        const uchar* lineA = a.constScanLine(aRow + y);
        const uchar* lineB = b.constScanLine(bRow + y);
        for (int x = 0; x < a.width(); ++x)
            total += qAbs(int(lineA[x]) - int(lineB[x]));
    }
    return total / (rows * a.width());
}

// 用原图抽样做二次校验，避免窄条特征在低纹理画面上误匹配
double sampledDifference(const QImage& a, int aRow, const QImage& b, int bRow, int rows)
{
    const int rowStep = qMax(1, rows / 80);
    const int columnStep = qMax(1, a.width() / 120);
    double total = 0;
    int samples = 0;
    for (int y = 0; y < rows; y += rowStep) {
        const QRgb* lineA = reinterpret_cast<const QRgb*>(a.constScanLine(aRow + y));
        const QRgb* lineB = reinterpret_cast<const QRgb*>(b.constScanLine(bRow + y));
        for (int x = 0; x < a.width(); x += columnStep) {
            total += qAbs(qGray(lineA[x]) - qGray(lineB[x]));
            ++samples;
        }
    }
    return samples > 0 ? total / samples : 1e9;
}
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
        const int overlap = detectOverlap(m_tail, frame);
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

int ScrollCapture::detectOverlap(const QImage& tail, const QImage& frame) const
{
    const int height = frame.height();
    const QImage tailSignature = rowSignature(tail);
    const QImage frameSignature = rowSignature(frame);

    // 画面完全没动：没有新内容
    if (rowDifference(tailSignature, 0, frameSignature, 0, height) < kSameFrameDifference)
        return -1;

    const int minOverlap = qMax(kMinOverlapRows, height / 20);
    const int maxOverlap = height - 4;

    // 粗筛：扫描全部偏移，按差异取全局最优的几个，再用原图精验
    // 只取扫描顺序靠前的候选会在平滑画面上选中错误的近邻偏移
    struct Candidate {
        int overlap;
        double difference;
    };
    QVector<Candidate> candidates;
    for (int overlap = maxOverlap; overlap >= minOverlap; --overlap) {
        const double difference =
            rowDifference(tailSignature, height - overlap, frameSignature, 0, overlap);
        if (difference <= kCoarseDifference)
            candidates.append({overlap, difference});
    }
    if (candidates.isEmpty())
        return 0;

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.difference < b.difference;
              });

    const int verifyCount = qMin(4, candidates.size());
    int bestOverlap = 0;
    double bestDifference = 1e9;
    for (int i = 0; i < verifyCount; ++i) {
        const Candidate& candidate = candidates.at(i);
        const double difference =
            sampledDifference(tail, height - candidate.overlap, frame, 0, candidate.overlap);
        if (difference < bestDifference) {
            bestDifference = difference;
            bestOverlap = candidate.overlap;
        }
    }

    if (bestOverlap == 0 || bestDifference > kVerifiedDifference)
        return 0;

    return bestOverlap;
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
