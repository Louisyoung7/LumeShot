// 模块 ui：界面层——全屏透明框选遮罩，选区旁带工具栏（确认 / 钉图 / 取消）。
#pragma once

#include <QPixmap>
#include <QRect>
#include <QWidget>

class SelectionOverlay : public QWidget {
    Q_OBJECT

public:
    SelectionOverlay(const QPixmap& desktop, const QRect& virtualGeometry,
                     QWidget* parent = nullptr);

signals:
    // 确认截图，capture 已裁剪好
    void regionSelected(const QPixmap& capture);
    // 钉图：capture 与它在桌面上的左上角位置
    void pinRequested(const QPixmap& capture, const QPoint& globalTopLeft);
    // 长截图：用户选中的桌面区域
    void longCaptureRequested(const QRect& globalRegion);
    // 一次性保存到用户选择的目录，capture 已裁剪好
    void saveRequested(const QPixmap& capture);
    void canceled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    enum class ToolButton {
        None = -1,
        Confirm = 0,
        Save = 1,
        Pin = 2,
        LongShot = 3,
        Cancel = 4
    };

    QRect currentSelection() const;
    QRect toolbarRect() const;
    QRect buttonRect(ToolButton button) const;
    ToolButton buttonAt(const QPoint& position) const;
    void triggerButton(ToolButton button);
    void confirmSelection();

    QPixmap m_desktop;
    QRect m_virtualGeometry;
    QPoint m_origin;
    QRect m_selection;
    ToolButton m_hoveredButton = ToolButton::None;
    ToolButton m_pressedButton = ToolButton::None;
    bool m_dragging = false;
};
