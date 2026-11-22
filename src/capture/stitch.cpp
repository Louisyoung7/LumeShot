#include "capture/stitch.h"

#include <algorithm>

namespace {

constexpr int kSignatureWidth = 48;           // 行特征宽度，越小比对越快
constexpr int kMinOverlapRows = 16;           // 最少重叠行数
constexpr double kSameFrameDifference = 1.2;  // 低于该值视为画面没动
constexpr double kCoarseDifference = 6.0;     // 粗筛阈值（行特征平均灰度差）
constexpr double kVerifiedDifference = 5.0;   // 精验阈值（原图采样平均灰度差）

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

namespace stitch {

int detectOverlap(const QImage& tail, const QImage& frame)
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

} // namespace stitch
