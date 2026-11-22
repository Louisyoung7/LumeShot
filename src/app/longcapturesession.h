// 模块 app：长截图会话——管理抓取、实时侧边栏、结果预览与钉图窗口的完整生命周期。
#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRect>

class LongCaptureFrame;
class LongCapturePreview;
class LongCaptureSidebar;
class ScrollCapture;

class LongCaptureSession : public QObject {
    Q_OBJECT

public:
    explicit LongCaptureSession(QObject* parent = nullptr);

    void start(const QRect& region);
    bool isActive() const { return m_active; }

signals:
    // 预览确认：image 为拼接结果，region 为最初框选的区域
    void finished(const QImage& image, const QRect& region);
    void canceled();

private:
    void closeWindows();
    void cleanup();
    void showPreview(const QImage& image);
    void pinImage(const QImage& image, const QPoint& preferred, const QRect& available);

    QPointer<LongCaptureFrame> m_frame;
    QPointer<LongCaptureSidebar> m_sidebar;
    QPointer<LongCapturePreview> m_preview;
    QPointer<ScrollCapture> m_scroll;
    QRect m_region;
    bool m_active = false;
};
