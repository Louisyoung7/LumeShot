// 模块 ui：截图工具栏图标绘制，选框工具栏与长截图预览侧边栏共用。
#pragma once

#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QPolygonF>
#include <QRect>

namespace toolbaricons {

inline void prepareIconPen(QPainter& painter, const QColor& color)
{
    QPen pen(color, 2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
}

inline void drawCheckIcon(QPainter& painter, const QRect& area, const QColor& color)
{
    prepareIconPen(painter, color);
    const QPointF center = area.center();
    painter.drawLine(center + QPointF(-6, 0), center + QPointF(-1, 5));
    painter.drawLine(center + QPointF(-1, 5), center + QPointF(6, -5));
}

inline void drawPinIcon(QPainter& painter, const QRect& area, const QColor& color)
{
    prepareIconPen(painter, color);
    const QPointF center = area.center();
    painter.drawLine(center + QPointF(-6, -7), center + QPointF(6, -7));

    QPolygonF body;
    body << center + QPointF(-5, -5) << center + QPointF(5, -5)
         << center + QPointF(2, 2) << center + QPointF(-2, 2);
    painter.drawPolygon(body);

    painter.drawLine(center + QPointF(0, 2), center + QPointF(0, 9));
}

inline void drawLongShotIcon(QPainter& painter, const QRect& area, const QColor& color)
{
    prepareIconPen(painter, color);
    const QPointF center = area.center();
    // 向下箭头，表示继续向下滚动截取
    painter.drawLine(center + QPointF(0, -8), center + QPointF(0, 7));
    painter.drawLine(center + QPointF(-4, 2), center + QPointF(0, 7));
    painter.drawLine(center + QPointF(4, 2), center + QPointF(0, 7));
}

inline void drawCrossIcon(QPainter& painter, const QRect& area, const QColor& color)
{
    prepareIconPen(painter, color);
    const QPointF center = area.center();
    painter.drawLine(center + QPointF(-5, -5), center + QPointF(5, 5));
    painter.drawLine(center + QPointF(-5, 5), center + QPointF(5, -5));
}

} // namespace toolbaricons
