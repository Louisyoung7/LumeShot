#include <QtTest>

#include <QElapsedTimer>

#include "capture/stitch.h"
#include "pagefixture.h"

using namespace fixture;

class TestStitch : public QObject {
    Q_OBJECT

private slots:
    void detectsScrolledOverlap();
    void detectsSlowScroll();
    void detectsOnePixelScroll();
    void detectsFastScroll();
    void identicalFrameReportsNoScroll();
    void unrelatedFrameReportsNoMatch();
    void overlapBelowMinimumIsRejected();
    void toleratesBrightnessShift();
    void toleratesNoise();
    void toleratesPinnedHeader();
    void toleratesAnimatedStrip();
    void ignoresScrollbar();
    void rejectsLowTexture();
    void repetitivePatternPrefersLargestOverlap();
    void rejectsHorizontalJitter();
    void rejectsReverseScroll();
    void rejectsSizeMismatch();
    void keepsPerformanceBudget();
};

void TestStitch::detectsScrolledOverlap()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0), pageFrame(40)), 260);
}

void TestStitch::detectsSlowScroll()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0), pageFrame(4)), 296);
}

void TestStitch::detectsOnePixelScroll()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0), pageFrame(1)), 299);
}

void TestStitch::detectsFastScroll()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0), pageFrame(280)), 20);
}

void TestStitch::identicalFrameReportsNoScroll()
{
    const QImage frame = pageFrame(0);
    QCOMPARE(stitch::detectOverlap(frame, frame), -1);
}

void TestStitch::unrelatedFrameReportsNoMatch()
{
    QImage black(kPageWidth, kFrameHeight, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage white(kPageWidth, kFrameHeight, QImage::Format_RGB32);
    white.fill(Qt::white);

    QCOMPARE(stitch::detectOverlap(black, white), 0);
}

void TestStitch::overlapBelowMinimumIsRejected()
{
    // 只重合 10 行，低于最少重叠行数
    QCOMPARE(stitch::detectOverlap(pageFrame(0), pageFrame(290)), 0);
}

void TestStitch::toleratesBrightnessShift()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0), withBrightness(pageFrame(40), 10)), 260);
}

void TestStitch::toleratesNoise()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0), withNoise(pageFrame(40), 2)), 260);
}

void TestStitch::toleratesPinnedHeader()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0, kFrameHeight, kPageWidth, true),
                                   pageFrame(40, kFrameHeight, kPageWidth, true)),
             260);
}

void TestStitch::toleratesAnimatedStrip()
{
    const QImage tail = withAnimation(pageFrame(0), 0);
    const QImage frame = withAnimation(pageFrame(40), 1);

    QCOMPARE(stitch::detectOverlap(tail, frame), 260);
}

void TestStitch::ignoresScrollbar()
{
    const QImage tail = withScrollbar(pageFrame(0), 0);
    const QImage frame = withScrollbar(pageFrame(40), 1);

    QCOMPARE(stitch::detectOverlap(tail, frame), 260);
}

void TestStitch::rejectsLowTexture()
{
    QCOMPARE(stitch::detectOverlap(lowTextureFrame(0), lowTextureFrame(40)), 0);
}

void TestStitch::repetitivePatternPrefersLargestOverlap()
{
    QCOMPARE(stitch::detectOverlap(repeatedFrame(0), repeatedFrame(40)), 260);
}

void TestStitch::rejectsHorizontalJitter()
{
    // 用户确认：横向抖动直接拒绝该帧，不做补偿
    QCOMPARE(stitch::detectOverlap(pageFrame(0), shiftedHorizontally(pageFrame(40), 2)), 0);
}

void TestStitch::rejectsReverseScroll()
{
    // 用户确认：反向滚动丢弃该帧，不回退
    QCOMPARE(stitch::detectOverlap(pageFrame(40), pageFrame(0)), 0);
}

void TestStitch::rejectsSizeMismatch()
{
    QCOMPARE(stitch::detectOverlap(pageFrame(0), pageFrame(40, kFrameHeight, kPageWidth - 4)),
             0);
}

void TestStitch::keepsPerformanceBudget()
{
    const QImage tail = pageFrame(0, 1080, 1920);
    const QImage frame = pageFrame(40, 1080, 1920);

    QElapsedTimer timer;
    timer.start();
    const int overlap = stitch::detectOverlap(tail, frame);
    const qint64 elapsed = timer.elapsed();

    QCOMPARE(overlap, 1040);
    QVERIFY2(elapsed < 100, qPrintable(QStringLiteral("耗时 %1ms").arg(elapsed)));
}

QTEST_GUILESS_MAIN(TestStitch)

#include "tst_stitch.moc"
