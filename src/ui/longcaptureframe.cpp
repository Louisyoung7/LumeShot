#include "ui/longcaptureframe.h"

#include "ui/selectionframe.h"

#include <QPainter>
#include <QRegion>

namespace {
constexpr int kMargin = 8;    // 窗口只包住选区外侧一小圈
constexpr int kBorderGap = 3; // 边框整体画在区域外侧，避免被截进长图
} // namespace

LongCaptureFrame::LongCaptureFrame(const QRect& region, const QRect& virtualGeometry,
                                   QWidget* parent)
    : QWidget(parent)
    , m_region(region.translated(-virtualGeometry.topLeft()))
{
    // 只覆盖选区外围一圈，点击穿透；覆盖整屏会让合成器进入直接扫描输出，
    // 抓屏和截图都会拿到卡住的旧画面
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::X11BypassWindowManagerHint | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::NoFocus);
    setGeometry(m_region.adjusted(-kMargin, -kMargin, kMargin, kMargin));

    // 挖掉选区本身，抓屏时才能看到下面的滚动内容
    QRegion shape(rect());
    shape -= QRegion(m_region.translated(-geometry().topLeft()));
    setMask(shape);
}

void LongCaptureFrame::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect region = m_region.translated(-geometry().topLeft());
    selectionframe::drawSelectionFrame(
        painter, region.adjusted(-kBorderGap, -kBorderGap, kBorderGap - 1, kBorderGap - 1));
}
