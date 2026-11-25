#include <QtTest>

#include <QImage>

#include "capture/scrollsession.h"
#include "pagefixture.h"

using namespace fixture;

class TestScrollSession : public QObject {
    Q_OBJECT

private slots:
    void appendsScrolledFrames();
    void ignoresIdenticalFrame();
    void discardsUnmatchedFrame();
    void ignoresFrameWithDifferentSize();
    void stopsAtHeightLimit();
    void keepsDevicePixelRatio();
    void ignoresEmptyFrame();
    void discardsBadFrameAndRecovers();
};

void TestScrollSession::appendsScrolledFrames()
{
    ScrollSession session;

    const ScrollSession::Result first = session.addFrame(pageFrame(0));
    QCOMPARE(first.outcome, ScrollSession::Outcome::Appended);
    QCOMPARE(session.totalHeight(), kFrameHeight);

    const ScrollSession::Result second = session.addFrame(pageFrame(40));
    QCOMPARE(second.outcome, ScrollSession::Outcome::Appended);
    QCOMPARE(second.piece.height(), 40); // 300 - 260 行重叠
    QCOMPARE(second.piece.pixel(10, 0), pageColor(kFrameHeight, 10));

    const ScrollSession::Result third = session.addFrame(pageFrame(80));
    QCOMPARE(third.outcome, ScrollSession::Outcome::Appended);
    QCOMPARE(session.totalHeight(), 380);

    const QImage result = session.compose();
    QCOMPARE(result.size(), QSize(kPageWidth, 380));

    // 接缝两侧的像素应与绝对行内容一致
    for (int y : {0, kFrameHeight - 1, kFrameHeight, 379})
        QCOMPARE(result.pixel(10, y), pageColor(y, 10));
}

void TestScrollSession::ignoresIdenticalFrame()
{
    ScrollSession session;
    session.addFrame(pageFrame(0));

    const ScrollSession::Result result = session.addFrame(pageFrame(0));
    QCOMPARE(result.outcome, ScrollSession::Outcome::NoScroll);
    QCOMPARE(session.totalHeight(), kFrameHeight);
}

void TestScrollSession::discardsUnmatchedFrame()
{
    QImage black(kPageWidth, kFrameHeight, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage white(kPageWidth, kFrameHeight, QImage::Format_RGB32);
    white.fill(Qt::white);

    ScrollSession session;
    session.addFrame(black);

    const ScrollSession::Result result = session.addFrame(white);
    QCOMPARE(result.outcome, ScrollSession::Outcome::NoMatch);
    QCOMPARE(session.totalHeight(), kFrameHeight);
}

void TestScrollSession::ignoresFrameWithDifferentSize()
{
    ScrollSession session;
    session.addFrame(pageFrame(0));

    const ScrollSession::Result result = session.addFrame(pageFrame(40, kFrameHeight, kPageWidth - 4));
    QCOMPARE(result.outcome, ScrollSession::Outcome::Ignored);
    QCOMPARE(session.totalHeight(), kFrameHeight);
}

void TestScrollSession::stopsAtHeightLimit()
{
    ScrollSession session(360);
    session.addFrame(pageFrame(0));  // 300
    session.addFrame(pageFrame(40)); // 340

    const ScrollSession::Result result = session.addFrame(pageFrame(80)); // 会到 380
    QCOMPARE(result.outcome, ScrollSession::Outcome::HeightLimit);
    QCOMPARE(session.totalHeight(), 340);
    QCOMPARE(session.compose().height(), 340);
}

void TestScrollSession::keepsDevicePixelRatio()
{
    QImage frame = pageFrame(0);
    frame.setDevicePixelRatio(2.0);

    ScrollSession session;
    session.addFrame(frame);

    QCOMPARE(session.devicePixelRatio(), 2.0);
    QCOMPARE(session.totalHeight(), kFrameHeight); // 物理像素
    QCOMPARE(session.compose().devicePixelRatio(), 2.0);
}

void TestScrollSession::ignoresEmptyFrame()
{
    ScrollSession session;
    QVERIFY(session.isEmpty());
    QVERIFY(session.compose().isNull());

    const ScrollSession::Result result = session.addFrame(QImage());
    QCOMPARE(result.outcome, ScrollSession::Outcome::Ignored);
    QVERIFY(session.isEmpty());
}

void TestScrollSession::discardsBadFrameAndRecovers()
{
    ScrollSession session;
    session.addFrame(pageFrame(0));
    session.addFrame(pageFrame(40));
    session.addFrame(pageFrame(80));
    QCOMPARE(session.totalHeight(), 380);

    // 横向抖动帧应被丢弃，已截内容不变
    const ScrollSession::Result bad =
        session.addFrame(shiftedHorizontally(pageFrame(120), 2));
    QCOMPARE(bad.outcome, ScrollSession::Outcome::NoMatch);
    QCOMPARE(session.totalHeight(), 380);

    // 下一帧正常，继续追加
    const ScrollSession::Result good = session.addFrame(pageFrame(120));
    QCOMPARE(good.outcome, ScrollSession::Outcome::Appended);
    QCOMPARE(session.totalHeight(), 420);

    const QImage result = session.compose();
    QCOMPARE(result.size(), QSize(kPageWidth, 420));
    for (int y : {379, 380, 419})
        QCOMPARE(result.pixel(10, y), pageColor(y, 10));
}

QTEST_GUILESS_MAIN(TestScrollSession)

#include "tst_scrollsession.moc"
