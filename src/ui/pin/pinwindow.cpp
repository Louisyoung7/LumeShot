#include "ui/pin/pinwindow.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>

namespace {
constexpr int kBorderWidth = 3;                        // 亮色边框宽度
constexpr int kEdgeWidth = 1;                          // 边框外侧的深色描边
constexpr qreal kMinScale = 0.1;                       // 缩小下限
constexpr qreal kMaxScale = 8.0;                       // 放大上限
const QColor kBorderColor(0x33, 0xaa, 0xff);
} // namespace

PinWindow::PinWindow(const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent)
    , m_sourcePixmap(pixmap)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose);
    setToolTip(QStringLiteral("拖动可移动，滚轮缩放，双击或右键可关闭钉图"));
    setCursor(Qt::OpenHandCursor);
    applyScale();
}

void PinWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    setCursor(Qt::ClosedHandCursor);
}

void PinWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging)
        return;
    move(event->globalPosition().toPoint() - m_dragOffset);
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
}

void PinWindow::applyScale()
{
    // 边框画在图片外围，不遮挡钉图内容
    const QSizeF base = m_sourcePixmap.deviceIndependentSize();
    const QSize content(qMax(1, qRound(base.width() * m_scale)),
                        qMax(1, qRound(base.height() * m_scale)));
    const qreal ratio = m_sourcePixmap.devicePixelRatio();
    m_pixmap = m_sourcePixmap.scaled(QSize(qRound(content.width() * ratio),
                                           qRound(content.height() * ratio)),
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_pixmap.setDevicePixelRatio(ratio);

    const int frame = kBorderWidth + kEdgeWidth;
    setFixedSize(content + QSize(frame * 2, frame * 2));
}

void PinWindow::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    const qreal target = qBound(kMinScale, m_scale * std::pow(1.1, delta / 120.0), kMaxScale);
    if (qFuzzyCompare(target, m_scale)) {
        event->accept();
        return;
    }

    // 以光标位置为锚点缩放：让光标下的图像内容保持在原地
    const QPoint globalPos = event->globalPosition().toPoint();
    const QPoint local = mapFromGlobal(globalPos);
    const int frame = kBorderWidth + kEdgeWidth;
    const QPointF anchor = (QPointF(local) - QPointF(frame, frame)) / m_scale;

    m_scale = target;
    applyScale();

    const QPointF newLocal = anchor * m_scale + QPointF(frame, frame);
    move(pos() + (globalPos - mapToGlobal(newLocal.toPoint())));
    event->accept();
}

void PinWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);

    // 外沿一道深色描边，再向内画亮色边框
    const int frame = kBorderWidth + kEdgeWidth;
    painter.fillRect(rect(), QColor(0, 0, 0, 160));
    painter.fillRect(rect().adjusted(kEdgeWidth, kEdgeWidth, -kEdgeWidth, -kEdgeWidth),
                     kBorderColor);

    painter.drawPixmap(QPoint(frame, frame), m_pixmap);
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        close();
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* closeAction = menu.addAction(QStringLiteral("关闭"));
    if (menu.exec(event->globalPos()) == closeAction)
        close();
}
