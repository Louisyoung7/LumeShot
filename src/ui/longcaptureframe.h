// 模块 ui：长截图期间的区域边框——点击穿透，包在选区外侧一圈，不挡抓屏。
#pragma once

#include <QRect>
#include <QWidget>

class LongCaptureFrame : public QWidget {
    Q_OBJECT

public:
    LongCaptureFrame(const QRect& region, const QRect& virtualGeometry,
                     QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRect m_region;
};
