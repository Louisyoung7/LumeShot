#include <QtTest>

#include <QImage>

#include "capture/stitch.h"

namespace {

constexpr int kWidth = 160;
constexpr int kHeight = 300;

// 每行颜色由绝对行号决定，滚动后同一内容行保持一致
QImage makeFrame(int startRow)
{
    QImage image(kWidth, kHeight, QImage::Format_RGB32);
    for (int y = 0; y < kHeight; ++y) {
        const int row = startRow + y;
        const QRgb color = qRgb((row * 37) % 251, (row * 91) % 241, (row * 151) % 239);
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < kWidth; ++x)
            line[x] = color;
    }
    return image;
}

} // namespace

class TestStitch : public QObject {
    Q_OBJECT

private slots:
    void detectsScrolledOverlap();
    void identicalFrameReportsNoScroll();
    void unrelatedFrameReportsNoMatch();
    void overlapBelowMinimumIsRejected();
};

void TestStitch::detectsScrolledOverlap()
{
    const QImage tail = makeFrame(0);
    const QImage frame = makeFrame(40);

    // 当前帧顶部有 260 行与上一帧底部重合
    QCOMPARE(stitch::detectOverlap(tail, frame), 260);
}

void TestStitch::identicalFrameReportsNoScroll()
{
    const QImage frame = makeFrame(0);
    QCOMPARE(stitch::detectOverlap(frame, frame), -1);
}

void TestStitch::unrelatedFrameReportsNoMatch()
{
    QImage tail(kWidth, kHeight, QImage::Format_RGB32);
    tail.fill(Qt::black);
    QImage frame(kWidth, kHeight, QImage::Format_RGB32);
    frame.fill(Qt::white);

    QCOMPARE(stitch::detectOverlap(tail, frame), 0);
}

void TestStitch::overlapBelowMinimumIsRejected()
{
    // 只重合 10 行，低于最少重叠行数，应判为不可靠
    const QImage tail = makeFrame(0);
    const QImage frame = makeFrame(290);

    QCOMPARE(stitch::detectOverlap(tail, frame), 0);
}

QTEST_GUILESS_MAIN(TestStitch)

#include "tst_stitch.moc"
