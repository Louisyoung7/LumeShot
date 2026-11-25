#include "capture/stitch.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

constexpr int kDescriptorWidth = 96;        // 描述子宽度，控制成本
constexpr int kColumnCount = 3;             // 横向分栏数
constexpr int kMinOverlapRows = 16;         // 最少重叠行数
constexpr int kSameRowDifference = 4;       // 单行视为一致的误差上限
constexpr int kSameRowPercent = 75;         // 一致行占比达到该值视为没滚动
constexpr int kContrastThreshold = 20;      // 区分平坦行
constexpr int kCoarseSamples = 32;          // 粗筛每栏采样行数
constexpr int kFineSamples = 128;           // 精验每栏采样行数
constexpr int kPixelTolerance = 6;          // 单像素容差，抗锯齿
constexpr double kColumnValidError = 6.0;   // 有效列的最大误差
constexpr double kColumnAgreementMargin = 0.5; // 各栏结论一致的误差容差
constexpr double kTieMargin = 1e-3;         // 判定为并列的误差差
constexpr int kTexturePercent = 25;         // 有效纹理行占比门槛
constexpr int kTopCandidatesPerColumn = 5;  // 每栏保留的粗筛候选数
constexpr int kCandidateExpand = 2;         // 候选向两侧扩展的行数
constexpr double kContentRowDifference = 4.0; // 与首行差异超过该值视为有内容

struct RowFeature {
    float mean = 0.0f;     // 行内均值
    float weighted = 0.0f; // 行内加权均值，弱化纯色像素
    float contrast = 0.0f; // 有效对比度，含纵向跨度
};

struct Profile {
    int width = 0;
    int height = 0;
    int margin = 0;
    int columnWidth = 0;
    QImage image; // 宽度缩到 96 的灰度图
    std::vector<RowFeature> rows;

    const RowFeature& feature(int row, int column) const
    {
        return rows[std::size_t(row) * kColumnCount + column];
    }

    int pixel(int row, int x) const { return image.constScanLine(row)[x]; }
};

// 把画面缩成窄条，每行每栏提取三个特征
Profile makeProfile(const QImage& source)
{
    Profile profile;
    profile.height = source.height();

    const QImage gray = source.convertToFormat(QImage::Format_Grayscale8);
    profile.image = gray.scaled(kDescriptorWidth, profile.height, Qt::IgnoreAspectRatio,
                                Qt::FastTransformation);
    profile.width = profile.image.width();
    profile.margin = std::min(3, profile.width / 12);
    profile.columnWidth = std::max(1, (profile.width - 2 * profile.margin) / kColumnCount);
    profile.rows.assign(std::size_t(profile.height) * kColumnCount, RowFeature{});

    for (int y = 0; y < profile.height; ++y) {
        const uchar* line = profile.image.constScanLine(y);
        for (int c = 0; c < kColumnCount; ++c) {
            int x0 = profile.margin + c * profile.columnWidth;
            int x1 = std::min(profile.width, x0 + profile.columnWidth);
            if (x1 <= x0) {
                x0 = 0;
                x1 = profile.width;
            }

            double sum = 0.0;
            int low = 255;
            int high = 0;
            for (int x = x0; x < x1; ++x) {
                const int v = line[x];
                sum += v;
                low = std::min(low, v);
                high = std::max(high, v);
            }

            const double count = x1 - x0;
            const double mean = sum / count;
            double weightedSum = 0.0;
            double weightSum = 0.0;
            for (int x = x0; x < x1; ++x) {
                const double weight = std::fabs(line[x] - mean) + 1.0;
                weightedSum += line[x] * weight;
                weightSum += weight;
            }

            RowFeature& feature = profile.rows[std::size_t(y) * kColumnCount + c];
            feature.mean = float(mean);
            feature.weighted = float(weightedSum / weightSum);
            feature.contrast = float(high - low);
        }
    }

    // 平坦行用上下邻近行的纵向跨度补强对比度
    for (int c = 0; c < kColumnCount; ++c) {
        for (int y = 0; y < profile.height; ++y) {
            RowFeature& feature = profile.rows[std::size_t(y) * kColumnCount + c];
            if (feature.contrast >= kContrastThreshold)
                continue;

            double span = 0.0;
            if (y > 0)
                span = std::max(span, std::fabs(double(feature.mean)
                                                 - double(profile.feature(y - 1, c).mean)));
            if (y + 1 < profile.height)
                span = std::max(span, std::fabs(double(feature.mean)
                                                 - double(profile.feature(y + 1, c).mean)));
            feature.contrast = std::max(feature.contrast, float(span));
        }
    }

    return profile;
}

// 有纹理行的纵向跨度，作为该栏的权重
int textureWeight(const Profile& profile, int column)
{
    int first = -1;
    int last = -1;
    for (int y = 0; y < profile.height; ++y) {
        if (profile.feature(y, column).contrast >= kContrastThreshold) {
            if (first < 0)
                first = y;
            last = y;
        }
    }
    return first < 0 ? 0 : last - first + 1;
}

// 纹理证据是否足够。门槛放宽到 25%，让窄栏或偏侧的内容也能参与匹配。
bool hasTexture(const Profile& profile)
{
    int total = 0;
    for (int c = 0; c < kColumnCount; ++c)
        total += textureWeight(profile, c);
    return total * 100 > profile.height * kColumnCount * kTexturePercent;
}

double median(std::vector<float> values)
{
    if (values.empty())
        return 0.0;

    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    const double upper = values[middle];
    if (values.size() % 2 == 1)
        return upper;

    std::nth_element(values.begin(), values.begin() + middle - 1, values.begin() + middle);
    return 0.5 * (values[middle - 1] + upper);
}

// 排序后丢掉最差 1/3，再取平均
double trimmedMean(std::vector<double> errors)
{
    if (errors.empty())
        return 1e9;

    std::sort(errors.begin(), errors.end());
    const std::size_t keep = std::max<std::size_t>(1, errors.size() * 2 / 3);
    double total = 0.0;
    for (std::size_t i = 0; i < keep; ++i)
        total += errors[i];
    return total / keep;
}

struct ColumnScore {
    double error = 1e9;       // 容差后的误差，用于判定列是否有效
    double strictError = 1e9; // 不容差的误差，用于区分相邻偏移
    int weight = 0;
};

// 粗筛：比较行特征，跳过两帧都平坦的行
ColumnScore scoreCoarse(const Profile& tail, const Profile& frame, int overlap, int column)
{
    ColumnScore score;
    const int height = tail.height;
    const int step = std::max(1, overlap / kCoarseSamples);

    std::vector<float> tailMean;
    std::vector<float> tailWeighted;
    std::vector<float> frameMean;
    std::vector<float> frameWeighted;
    int firstRow = -1;
    int lastRow = -1;

    for (int y = 0; y < overlap; y += step) {
        const int tailRow = height - overlap + y;
        const RowFeature& a = tail.feature(tailRow, column);
        const RowFeature& b = frame.feature(y, column);
        if (a.contrast < kContrastThreshold && b.contrast < kContrastThreshold)
            continue;

        tailMean.push_back(a.mean);
        tailWeighted.push_back(a.weighted);
        frameMean.push_back(b.mean);
        frameWeighted.push_back(b.weighted);
        if (firstRow < 0)
            firstRow = tailRow;
        lastRow = tailRow;
    }

    if (firstRow < 0)
        return score;

    const double tailMeanBase = median(tailMean);
    const double tailWeightedBase = median(tailWeighted);
    const double frameMeanBase = median(frameMean);
    const double frameWeightedBase = median(frameWeighted);

    std::vector<double> errors;
    errors.reserve(tailMean.size());
    for (std::size_t i = 0; i < tailMean.size(); ++i) {
        const double meanDifference =
            std::fabs((tailMean[i] - tailMeanBase) - (frameMean[i] - frameMeanBase));
        const double weightedDifference =
            std::fabs((tailWeighted[i] - tailWeightedBase)
                      - (frameWeighted[i] - frameWeightedBase));
        errors.push_back(0.5 * (meanDifference + weightedDifference));
    }

    score.error = trimmedMean(std::move(errors));
    score.weight = lastRow - firstRow + 1;
    return score;
}

// 精验：逐像素比较，行内先去掉均值，再用容差削弱噪声
ColumnScore scoreFine(const Profile& tail, const Profile& frame, int overlap, int column)
{
    ColumnScore score;
    const int height = tail.height;
    const int step = std::max(1, overlap / kFineSamples);

    int x0 = tail.margin + column * tail.columnWidth;
    int x1 = std::min(tail.width, x0 + tail.columnWidth);
    if (x1 <= x0) {
        x0 = 0;
        x1 = tail.width;
    }
    const double count = x1 - x0;

    std::vector<double> errors;
    std::vector<double> strictErrors;
    int firstRow = -1;
    int lastRow = -1;

    for (int y = 0; y < overlap; y += step) {
        const int tailRow = height - overlap + y;
        const RowFeature& a = tail.feature(tailRow, column);
        const RowFeature& b = frame.feature(y, column);
        if (a.contrast < kContrastThreshold && b.contrast < kContrastThreshold)
            continue;

        double tailSum = 0.0;
        double frameSum = 0.0;
        for (int x = x0; x < x1; ++x) {
            tailSum += tail.pixel(tailRow, x);
            frameSum += frame.pixel(y, x);
        }
        const double tailBase = tailSum / count;
        const double frameBase = frameSum / count;

        double error = 0.0;
        double strictError = 0.0;
        for (int x = x0; x < x1; ++x) {
            const double difference =
                std::fabs((tail.pixel(tailRow, x) - tailBase)
                          - (frame.pixel(y, x) - frameBase));
            error += std::max(0.0, difference - kPixelTolerance);
            strictError += difference;
        }

        errors.push_back(error / count);
        strictErrors.push_back(strictError / count);
        if (firstRow < 0)
            firstRow = tailRow;
        lastRow = tailRow;
    }

    if (firstRow < 0)
        return score;

    score.error = trimmedMean(std::move(errors));
    score.strictError = trimmedMean(std::move(strictErrors));
    score.weight = lastRow - firstRow + 1;
    return score;
}

// 判定两帧是否基本一致：逐行比较，允许小面积动画
bool mostlySame(const Profile& a, const Profile& b)
{
    if (a.height != b.height || a.height == 0)
        return false;

    double meanBase[kColumnCount] = {};
    double weightedBase[kColumnCount] = {};
    for (int c = 0; c < kColumnCount; ++c) {
        for (int y = 0; y < a.height; ++y) {
            meanBase[c] += a.feature(y, c).mean - b.feature(y, c).mean;
            weightedBase[c] += a.feature(y, c).weighted - b.feature(y, c).weighted;
        }
        meanBase[c] /= a.height;
        weightedBase[c] /= a.height;
    }

    int sameRows = 0;
    for (int y = 0; y < a.height; ++y) {
        double rowError = 0.0;
        for (int c = 0; c < kColumnCount; ++c) {
            const RowFeature& fa = a.feature(y, c);
            const RowFeature& fb = b.feature(y, c);
            rowError += std::fabs((fa.mean - fb.mean) - meanBase[c]);
            rowError += std::fabs((fa.weighted - fb.weighted) - weightedBase[c]);
        }
        rowError /= kColumnCount * 2;
        if (rowError < kSameRowDifference)
            ++sameRows;
    }

    return sameRows * 100 >= a.height * kSameRowPercent;
}

struct CoarseResult {
    int overlap = 0;
    ColumnScore columns[kColumnCount];
};

struct CandidateScore {
    int overlap = 0;
    double strictError = 1e9; // 不容差误差，用于区分相邻偏移
};

} // namespace

namespace stitch {

int detectOverlap(const QImage& tail, const QImage& frame)
{
    if (tail.isNull() || frame.isNull() || tail.size() != frame.size())
        return 0;

    const int width = frame.width();
    const int height = frame.height();
    if (width < 3 || height < kMinOverlapRows + 1)
        return 0;

    const Profile tailProfile = makeProfile(tail);
    const Profile frameProfile = makeProfile(frame);

    // 低纹理画面没有足够证据，直接丢弃
    if (!hasTexture(tailProfile))
        return 0;

    if (mostlySame(tailProfile, frameProfile))
        return -1;

    const int minOverlap = std::max(kMinOverlapRows, height / 20);
    const int maxOverlap = height - 1;
    if (minOverlap > maxOverlap)
        return 0;

    // 粗筛：逐偏移打分，每栏保留最好的几个候选
    std::vector<CoarseResult> coarse;
    coarse.reserve(std::size_t(maxOverlap - minOverlap + 1));
    for (int overlap = maxOverlap; overlap >= minOverlap; --overlap) {
        CoarseResult result;
        result.overlap = overlap;
        for (int c = 0; c < kColumnCount; ++c)
            result.columns[c] = scoreCoarse(tailProfile, frameProfile, overlap, c);
        coarse.push_back(result);
    }

    std::vector<int> candidates;
    for (int c = 0; c < kColumnCount; ++c) {
        std::vector<std::size_t> order(coarse.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;

        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const ColumnScore& left = coarse[a].columns[c];
            const ColumnScore& right = coarse[b].columns[c];
            if (left.error != right.error)
                return left.error < right.error;
            // 误差相同时偏好重叠更大的偏移
            return coarse[a].overlap > coarse[b].overlap;
        });

        const int keep = std::min<int>(kTopCandidatesPerColumn, int(order.size()));
        for (int i = 0; i < keep; ++i) {
            const CoarseResult& result = coarse[order[i]];
            if (result.columns[c].weight == 0)
                continue;
            for (int delta = -kCandidateExpand; delta <= kCandidateExpand; ++delta) {
                const int overlap =
                    std::clamp(result.overlap + delta, minOverlap, maxOverlap);
                candidates.push_back(overlap);
            }
        }
    }

    if (candidates.empty())
        return 0;

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    // 精验所有候选
    struct FineCandidate {
        int overlap = 0;
        ColumnScore columns[kColumnCount];
    };
    std::vector<FineCandidate> fine;
    fine.reserve(candidates.size());

    double minima[kColumnCount];
    std::fill_n(minima, kColumnCount, 1e9);

    for (int overlap : candidates) {
        FineCandidate candidate;
        candidate.overlap = overlap;
        for (int c = 0; c < kColumnCount; ++c) {
            candidate.columns[c] = scoreFine(tailProfile, frameProfile, overlap, c);
            if (candidate.columns[c].weight > 0)
                minima[c] = std::min(minima[c], candidate.columns[c].error);
        }
        fine.push_back(candidate);
    }

    // 有效列：该栏有纹理，且最好误差足够小
    bool columnValid[kColumnCount];
    for (int c = 0; c < kColumnCount; ++c)
        columnValid[c] = minima[c] <= kColumnValidError;

    // 候选仲裁：列间一致才通过，再用不容差误差区分相邻偏移
    double bestScore = 1e9;
    int bestOverlap = 0;
    std::vector<CandidateScore> consistent;
    consistent.reserve(fine.size());

    for (const FineCandidate& candidate : fine) {
        const int totalWeight = candidate.overlap * kColumnCount;
        double agreeingWeight = 0.0;
        double weightedStrict = 0.0;

        for (int c = 0; c < kColumnCount; ++c) {
            const ColumnScore& column = candidate.columns[c];
            if (!columnValid[c] || column.weight == 0)
                continue;
            if (column.error > minima[c] + kColumnAgreementMargin)
                continue;
            agreeingWeight += column.weight;
            weightedStrict += column.weight * column.strictError;
        }

        if (agreeingWeight * 100 <= totalWeight * kTexturePercent)
            continue;

        const double strictScore = weightedStrict / agreeingWeight;
        consistent.push_back({candidate.overlap, strictScore});
        if (strictScore < bestScore) {
            bestScore = strictScore;
            bestOverlap = candidate.overlap;
        }
    }

    if (bestOverlap == 0)
        return 0;

    // 严格误差真正并列时才取重叠最大的候选，避免在真实页面上误选更大的重叠
    int chosen = bestOverlap;
    for (const CandidateScore& score : consistent) {
        if (score.strictError <= bestScore + kTieMargin && score.overlap > chosen)
            chosen = score.overlap;
    }

    return chosen;
}

bool hasReliableTexture(const QImage& frame)
{
    if (frame.isNull() || frame.width() < 3 || frame.height() < kMinOverlapRows + 1)
        return false;

    return hasTexture(makeProfile(frame));
}

bool framesMostlySame(const QImage& previous, const QImage& current)
{
    if (previous.isNull() || current.isNull() || previous.size() != current.size())
        return false;

    return mostlySame(makeProfile(previous), makeProfile(current));
}

bool hasVisibleContent(const QImage& strip)
{
    if (strip.isNull() || strip.height() == 0)
        return false;

    const QImage gray = strip.convertToFormat(QImage::Format_Grayscale8)
                            .scaled(64, strip.height(), Qt::IgnoreAspectRatio,
                                    Qt::FastTransformation);
    const int width = gray.width();
    if (width == 0)
        return false;

    // 以首行为基准逐行比较：文字、图片等内容行的灰度分布必然不同，
    // 而回弹露出的背景、纯色条每行几乎一致。
    const uchar* first = gray.constScanLine(0);
    for (int y = 1; y < gray.height(); ++y) {
        const uchar* line = gray.constScanLine(y);
        double difference = 0.0;
        for (int x = 0; x < width; ++x)
            difference += std::fabs(double(line[x]) - double(first[x]));
        if (difference / width >= kContentRowDifference)
            return true;
    }
    return false;
}

} // namespace stitch
