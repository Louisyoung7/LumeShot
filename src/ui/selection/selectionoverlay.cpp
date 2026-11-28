#include "ui/selection/selectionoverlay.h"

#include "capture/screencapture.h"
#include "ui/common/selectionframe.h"
#include "ui/common/toolbaricons.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

namespace {
constexpr int kMinimumSelection = 4;   // 小于该尺寸视为单击
constexpr int kButtonSize = 36;
constexpr int kButtonGap = 4;
constexpr int kToolbarOffset = 8;      // 工具栏与选区的间距
constexpr int kScreenMargin = 10;

constexpr QColor kAccentColor(0x33, 0xaa, 0xff);
} // namespace

SelectionOverlay::SelectionOverlay(const QPixmap& desktop, const QRect& virtualGeometry,
                                   QWidget* parent)
    : QWidget(parent)
    , m_desktop(desktop)
    , m_virtualGeometry(virtualGeometry)
{
    // override-redirect 窗口：绕过窗口管理器，避免跨屏遮罩被 GNOME 缩放或挪位
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::X11BypassWindowManagerHint);
    // 透明背景避免全屏窗口 map/unmap 时合成器闪一帧黑屏
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    setGeometry(m_virtualGeometry);
}

void SelectionOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(0, 0, 0, 110));

    const QRect selection = m_selection.normalized().intersected(rect());
    if (selection.isEmpty())
        return;

    // 擦掉选区内的暗化层，再画回进入截图时的快照，保持预览与裁剪结果一致
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.fillRect(selection, QColor(0, 0, 0, 255));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const qreal scale = m_desktop.devicePixelRatio();
    const QRectF source(selection.x() * scale, selection.y() * scale,
                        selection.width() * scale, selection.height() * scale);
    painter.drawPixmap(QRectF(selection), m_desktop, source);

    selectionframe::drawSelectionFrame(painter, selection);

    // 尺寸提示，贴着选区左上角显示
    const QString text = QStringLiteral("%1 × %2").arg(selection.width()).arg(selection.height());
    QFont font = painter.font();
    font.setPointSizeF(10);
    painter.setFont(font);
    QRect labelRect = QFontMetrics(font).boundingRect(text).adjusted(-8, -4, 8, 4);
    QPoint labelPos = selection.topLeft() + QPoint(0, -labelRect.height() - 4);
    if (labelPos.y() < 0)
        labelPos = selection.topLeft() + QPoint(4, 4);
    if (labelPos.x() + labelRect.width() > width())
        labelPos.setX(width() - labelRect.width() - 4);
    labelRect.moveTopLeft(labelPos);

    painter.fillRect(labelRect, QColor(0, 0, 0, 170));
    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, text);

    // 拖拽过程中不显示工具栏，松手后再出现
    if (m_dragging)
        return;

    const QRect bar = toolbarRect();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 170));
    painter.drawRoundedRect(bar, 8, 8);

    for (ToolButton button :
         {ToolButton::Confirm, ToolButton::Pin, ToolButton::LongShot, ToolButton::Cancel}) {
        const QRect area = buttonRect(button);
        if (button == m_hoveredButton || button == m_pressedButton) {
            painter.setBrush(QColor(255, 255, 255, m_pressedButton == button ? 70 : 35));
            painter.drawRoundedRect(area.adjusted(2, 2, -2, -2), 6, 6);
        }
        const QColor iconColor = button == ToolButton::LongShot
                                     ? QColor(0xff, 0xa5, 0x33)
                                     : (button == ToolButton::Pin ? QColor(0x33, 0xaa, 0xff)
                                                                  : QColor(Qt::white));
        switch (button) {
        case ToolButton::Confirm:
            toolbaricons::drawCheckIcon(painter, area, iconColor);
            break;
        case ToolButton::Pin:
            toolbaricons::drawPinIcon(painter, area, iconColor);
            break;
        case ToolButton::LongShot:
            toolbaricons::drawLongShotIcon(painter, area, iconColor);
            break;
        case ToolButton::Cancel:
            toolbaricons::drawCrossIcon(painter, area, iconColor);
            break;
        default:
            break;
        }
    }
}

void SelectionOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        emit canceled();
        close();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;

    const ToolButton button = buttonAt(event->position().toPoint());
    if (button != ToolButton::None) {
        m_pressedButton = button;
        update();
        return;
    }

    m_dragging = true;
    m_pressedButton = ToolButton::None;
    m_origin = event->position().toPoint();
    m_selection = QRect(m_origin, QSize());
    update();
}

void SelectionOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_selection = QRect(m_origin, event->position().toPoint()).normalized().intersected(rect());
        update();
        return;
    }

    const ToolButton hovered = buttonAt(event->position().toPoint());
    if (hovered != m_hoveredButton) {
        m_hoveredButton = hovered;
        setCursor(hovered == ToolButton::None ? Qt::CrossCursor : Qt::PointingHandCursor);
        update();
    }
}

void SelectionOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pressedButton != ToolButton::None) {
        const ToolButton pressed = m_pressedButton;
        m_pressedButton = ToolButton::None;
        if (buttonAt(event->position().toPoint()) == pressed)
            triggerButton(pressed);
        else
            update();
        return;
    }

    if (!m_dragging)
        return;
    m_dragging = false;

    if (event->button() != Qt::LeftButton) {
        emit canceled();
        close();
        return;
    }

    // 单击时选中光标所在的整块屏幕，松手后同样保留选区等待确认
    const QRect selection = currentSelection();
    if (selection.width() < kMinimumSelection || selection.height() < kMinimumSelection) {
        QScreen* screen = QGuiApplication::screenAt(mapToGlobal(event->position().toPoint()));
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (!screen) {
            emit canceled();
            close();
            return;
        }
        m_selection =
            screen->geometry().translated(-m_virtualGeometry.topLeft()).intersected(rect());
    }
    update();
}

void SelectionOverlay::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        emit canceled();
        close();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        confirmSelection();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void SelectionOverlay::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    raise();
    // 绕过窗口管理器后不会自动获得焦点，显式抓取键盘
    grabKeyboard();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
}

void SelectionOverlay::hideEvent(QHideEvent* event)
{
    releaseKeyboard();
    QWidget::hideEvent(event);
}

QRect SelectionOverlay::currentSelection() const
{
    return m_selection.normalized().intersected(rect());
}

QRect SelectionOverlay::toolbarRect() const
{
    const QRect selection = currentSelection();
    if (selection.isEmpty())
        return {};

    const int toolbarHeight = kButtonSize * 4 + kButtonGap * 3;

    // 优先放在选区右侧，右侧放不下就翻到左侧
    int x = selection.right() + kToolbarOffset;
    if (x + kButtonSize > width() - kScreenMargin)
        x = selection.left() - kToolbarOffset - kButtonSize;
    x = qBound(kScreenMargin, x, width() - kButtonSize - kScreenMargin);

    int y = selection.center().y() - toolbarHeight / 2;
    y = qBound(kScreenMargin, y, height() - toolbarHeight - kScreenMargin);

    return QRect(x, y, kButtonSize, toolbarHeight);
}

QRect SelectionOverlay::buttonRect(ToolButton button) const
{
    if (button == ToolButton::None)
        return {};
    const QRect bar = toolbarRect();
    if (bar.isEmpty())
        return {};
    const int offset = int(button) * (kButtonSize + kButtonGap);
    return QRect(bar.x(), bar.y() + offset, kButtonSize, kButtonSize);
}

SelectionOverlay::ToolButton SelectionOverlay::buttonAt(const QPoint& position) const
{
    if (m_dragging || currentSelection().isEmpty())
        return ToolButton::None;
    for (ToolButton button :
         {ToolButton::Confirm, ToolButton::Pin, ToolButton::LongShot, ToolButton::Cancel}) {
        if (buttonRect(button).contains(position))
            return button;
    }
    return ToolButton::None;
}

void SelectionOverlay::triggerButton(ToolButton button)
{
    switch (button) {
    case ToolButton::Confirm:
        confirmSelection();
        break;
    case ToolButton::Pin: {
        const QRect selection = currentSelection();
        if (selection.isEmpty()) {
            emit canceled();
            close();
            break;
        }
        const QPixmap capture = ScreenCapture::crop(m_desktop, selection);
        const QPoint position = m_virtualGeometry.topLeft() + selection.topLeft();
        // 先收起遮罩，钉图窗口再出现，避免遮罩盖在钉图上方
        close();
        emit pinRequested(capture, position);
        break;
    }
    case ToolButton::LongShot: {
        const QRect selection = currentSelection();
        if (selection.isEmpty()) {
            emit canceled();
            close();
            break;
        }
        const QRect region(m_virtualGeometry.topLeft() + selection.topLeft(), selection.size());
        close();
        emit longCaptureRequested(region);
        break;
    }
    case ToolButton::Cancel:
        emit canceled();
        close();
        break;
    default:
        break;
    }
}

void SelectionOverlay::confirmSelection()
{
    const QRect selection = currentSelection();
    if (selection.isEmpty()) {
        emit canceled();
        close();
        return;
    }
    emit regionSelected(ScreenCapture::crop(m_desktop, selection));
    close();
}
