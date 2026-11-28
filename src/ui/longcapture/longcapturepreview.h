// 模块 ui：长截图预览——展示拼接结果，右侧边栏可确认、钉图或取消。
#pragma once

#include <QImage>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

class LongCapturePreview : public QWidget {
    Q_OBJECT

public:
    // available 用于计算预览的最大显示尺寸（通常是选区所在屏幕的可用区域）
    LongCapturePreview(const QImage& image, const QRect& available, QWidget* parent = nullptr);

signals:
    void accepted();      // 确认：交给 AppController 复制/保存
    void pinRequested();  // 钉图
    void saveRequested(); // 一次性保存到用户选择的目录
    void canceled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    enum class Button { Confirm = 0, Save = 1, Pin = 2, Cancel = 3 };
    static constexpr Button kNoButton = static_cast<Button>(-1);

    void applyDisplaySize();
    QRect imageRect() const;
    QRect toolbarRect() const;
    QRect buttonRect(Button button) const;
    Button buttonAt(const QPoint& position) const;
    void triggerButton(Button button);

    QImage m_image;
    QSize m_baseSize;     // 按屏幕适配出的初始显示尺寸
    QSize m_displaySize;  // 缩放后的显示尺寸（逻辑像素）
    qreal m_zoom = 1.0;
    Button m_hoveredButton = kNoButton;
    Button m_pressedButton = kNoButton;
    bool m_pressingButton = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
};
