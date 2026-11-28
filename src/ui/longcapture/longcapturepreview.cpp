#include "ui/longcapture/longcapturepreview.h"

#include "ui/common/toolbaricons.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QWheelEvent>

#include <cmath>

namespace {
constexpr int kBorderWidth = 3;              // 亮色边框宽度
constexpr int kEdgeWidth = 1;                // 边框外侧的深色描边
constexpr int kFrame = kBorderWidth + kEdgeWidth;
constexpr int kButtonSize = 36;
constexpr int kButtonGap = 4;
constexpr int kSidebarGap = 8;               // 图片与侧边栏的间距
constexpr int kToolbarHeight = kButtonSize * 4 + kButtonGap * 3;
constexpr qreal kMaxWidthRatio = 0.7;        // 预览占屏幕可用区域的最大比例
constexpr qreal kMaxHeightRatio = 0.85;
constexpr qreal kMinZoom = 0.1;             // 相对初始适配尺寸的缩放范围
constexpr qreal kMaxZoom = 8.0;
const QColor kAccentColor(0x33, 0xaa, 0xff);
} // namespace

LongCapturePreview::LongCapturePreview(const QImage& image, const QRect& available,
                                       QWidget* parent)
    : QWidget(parent)
    , m_image(image)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(QStringLiteral("Enter 确认，Esc 取消，滚轮缩放，拖动可移动预览"));

    const qreal dpr = m_image.devicePixelRatio() > 0.0 ? m_image.devicePixelRatio() : 1.0;
    const QSize logical(qMax(1, qRound(m_image.width() / dpr)),
                        qMax(1, qRound(m_image.height() / dpr)));
    const qreal scale =
        qMin(1.0, qMin(available.width() * kMaxWidthRatio / logical.width(),
                      available.height() * kMaxHeightRatio / logical.height()));
    m_baseSize = (QSizeF(logical) * scale).toSize();
    m_displaySize = m_baseSize;
    applyDisplaySize();
}

void LongCapturePreview::applyDisplaySize()
{
    const int height = qMax(m_displaySize.height() + kFrame * 2, kToolbarHeight + kFrame * 2);
    setFixedSize(m_displaySize.width() + kFrame * 2 + kSidebarGap + kButtonSize, height);
}

void LongCapturePreview::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 与钉图一致的边框：深色外沿 + 亮色内圈
    const QRect image = imageRect();
    const QRect frame = image.adjusted(-kFrame, -kFrame, kFrame, kFrame);
    painter.fillRect(frame, QColor(0, 0, 0, 160));
    painter.fillRect(frame.adjusted(kEdgeWidth, kEdgeWidth, -kEdgeWidth, -kEdgeWidth),
                     kAccentColor);

    painter.drawImage(QRectF(image), m_image, QRectF(m_image.rect()));

    // 右侧边栏
    const QRect bar = toolbarRect();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 170));
    painter.drawRoundedRect(bar, 8, 8);

    for (Button button : {Button::Confirm, Button::Save, Button::Pin, Button::Cancel}) {
        const QRect area = buttonRect(button);
        if (button == m_hoveredButton) {
            const bool pressed = m_pressingButton && m_pressedButton == button;
            painter.setBrush(QColor(255, 255, 255, pressed ? 70 : 35));
            painter.drawRoundedRect(area.adjusted(2, 2, -2, -2), 6, 6);
        }
        const QColor iconColor = button == Button::Pin ? kAccentColor : QColor(Qt::white);
        switch (button) {
        case Button::Confirm:
            toolbaricons::drawCheckIcon(painter, area, iconColor);
            break;
        case Button::Save:
            toolbaricons::drawSaveIcon(painter, area, iconColor);
            break;
        case Button::Pin:
            toolbaricons::drawPinIcon(painter, area, iconColor);
            break;
        case Button::Cancel:
            toolbaricons::drawCrossIcon(painter, area, iconColor);
            break;
        default:
            break;
        }
    }
}

void LongCapturePreview::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        triggerButton(Button::Confirm);
        return;
    case Qt::Key_Escape:
        triggerButton(Button::Cancel);
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void LongCapturePreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const Button button = buttonAt(event->position().toPoint());
    if (button != kNoButton) {
        m_pressingButton = true;
        m_pressedButton = button;
        update();
        return;
    }

    // 拖动图片区域移动整个预览窗口
    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    setCursor(Qt::ClosedHandCursor);
}

void LongCapturePreview::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        return;
    }

    const Button hovered = buttonAt(event->position().toPoint());
    if (hovered == m_hoveredButton)
        return;

    m_hoveredButton = hovered;
    setCursor(hovered == kNoButton ? Qt::OpenHandCursor : Qt::PointingHandCursor);
    update();
}

void LongCapturePreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pressingButton) {
        const Button pressed = m_pressedButton;
        m_pressingButton = false;
        m_pressedButton = kNoButton;
        if (buttonAt(event->position().toPoint()) == pressed)
            triggerButton(pressed);
        else
            update();
        return;
    }

    if (!m_dragging)
        return;
    m_dragging = false;
    setCursor(m_hoveredButton == kNoButton ? Qt::OpenHandCursor : Qt::PointingHandCursor);
}

void LongCapturePreview::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    const qreal target = qBound(kMinZoom, m_zoom * std::pow(1.1, delta / 120.0), kMaxZoom);
    if (qFuzzyCompare(target, m_zoom)) {
        event->accept();
        return;
    }

    // 以光标下的图像位置为锚点缩放，与钉图保持一致
    const QPoint globalPos = event->globalPosition().toPoint();
    const QPoint local = mapFromGlobal(globalPos);
    const qreal anchorX = qreal(local.x() - kFrame) / m_displaySize.width();
    const qreal anchorY = qreal(local.y() - kFrame) / m_displaySize.height();

    m_zoom = target;
    m_displaySize = (QSizeF(m_baseSize) * m_zoom).toSize();
    applyDisplaySize();

    const QPointF newLocal(kFrame + anchorX * m_displaySize.width(),
                           kFrame + anchorY * m_displaySize.height());
    move(pos() + (globalPos - mapToGlobal(newLocal.toPoint())));
    event->accept();
}

void LongCapturePreview::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
}

QRect LongCapturePreview::imageRect() const
{
    return QRect(QPoint(kFrame, kFrame), m_displaySize);
}

QRect LongCapturePreview::toolbarRect() const
{
    const QRect image = imageRect();
    const int x = image.right() + 1 + kSidebarGap;
    const int y = qBound(kFrame, image.center().y() - kToolbarHeight / 2,
                         height() - kToolbarHeight - kFrame);
    return QRect(x, y, kButtonSize, kToolbarHeight);
}

QRect LongCapturePreview::buttonRect(Button button) const
{
    const QRect bar = toolbarRect();
    const int offset = int(button) * (kButtonSize + kButtonGap);
    return QRect(bar.x(), bar.y() + offset, kButtonSize, kButtonSize);
}

LongCapturePreview::Button LongCapturePreview::buttonAt(const QPoint& position) const
{
    for (Button button : {Button::Confirm, Button::Save, Button::Pin, Button::Cancel}) {
        if (buttonRect(button).contains(position))
            return button;
    }
    return kNoButton;
}

void LongCapturePreview::triggerButton(Button button)
{
    switch (button) {
    case Button::Confirm:
        emit accepted();
        break;
    case Button::Save:
        emit saveRequested();
        break;
    case Button::Pin:
        emit pinRequested();
        break;
    case Button::Cancel:
        emit canceled();
        break;
    default:
        return;
    }
    close();
}
