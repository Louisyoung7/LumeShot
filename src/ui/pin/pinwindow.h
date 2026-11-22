// 模块 ui：钉图窗口——把选区固定在最顶层，双击或右键菜单关闭，滚轮缩放。
#pragma once

#include <QPixmap>
#include <QWidget>

class PinWindow : public QWidget {
    Q_OBJECT

public:
    explicit PinWindow(const QPixmap& pixmap, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void applyScale();

    QPixmap m_sourcePixmap;  // 原始图片，缩放始终基于它，避免反复插值失真
    QPixmap m_pixmap;        // 当前缩放后的显示图片
    qreal m_scale = 1.0;
    QPoint m_dragOffset;
    bool m_dragging = false;
};
