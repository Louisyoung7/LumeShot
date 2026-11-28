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

    // 外部入口：侧边栏不可用时（例如被移出屏幕）由托盘菜单触发
    void finish();
    void cancel();

signals:
    // 预览确认：image 为拼接结果，region 为最初框选的区域
    void finished(const QImage& image, const QRect& region);
    // 预览中选择保存位置：image 为拼接结果
    void saveRequested(const QImage& image);
    void canceled();
    // 抓取阶段结束（预览尚未处理），用于收尾托盘里的长截图菜单
    void captureEnded();

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
