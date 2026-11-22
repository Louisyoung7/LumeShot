// 模块 ui：选区边框绘制，框选遮罩与长截图边框共用。
#pragma once

#include <QColor>
#include <QPainter>
#include <QRect>

namespace selectionframe {

inline QColor accentColor()
{
    return QColor(0x33, 0xaa, 0xff);
}

// 深色外圈 + 亮色内圈 + 四角把手，保证在深浅背景上都清晰可见
inline void drawSelectionFrame(QPainter& painter, const QRect& selection)
{
    const QColor accent = accentColor();
    painter.setBrush(Qt::NoBrush);

    painter.setPen(QPen(QColor(0, 0, 0, 140), 3));
    painter.drawRect(QRectF(selection).adjusted(-1.0, -1.0, 1.0, 1.0));

    painter.setPen(QPen(accent, 2));
    painter.drawRect(QRectF(selection).adjusted(0.5, 0.5, -0.5, -0.5));

    const int arm = qBound(6, qMin(selection.width(), selection.height()) / 3, 18);
    QPen handlePen(accent, 3);
    handlePen.setCapStyle(Qt::FlatCap);
    painter.setPen(handlePen);

    const QPoint topLeft = selection.topLeft();
    const QPoint topRight = selection.topRight();
    const QPoint bottomLeft = selection.bottomLeft();
    const QPoint bottomRight = selection.bottomRight();

    painter.drawLine(topLeft, topLeft + QPoint(arm, 0));
    painter.drawLine(topLeft, topLeft + QPoint(0, arm));

    painter.drawLine(topRight, topRight + QPoint(-arm, 0));
    painter.drawLine(topRight, topRight + QPoint(0, arm));

    painter.drawLine(bottomLeft, bottomLeft + QPoint(arm, 0));
    painter.drawLine(bottomLeft, bottomLeft + QPoint(0, -arm));

    painter.drawLine(bottomRight, bottomRight + QPoint(-arm, 0));
    painter.drawLine(bottomRight, bottomRight + QPoint(0, -arm));
}

} // namespace selectionframe
