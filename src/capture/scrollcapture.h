// 模块 capture：长截图——定时抓取固定区域，交给 ScrollSession 做重叠判定与拼接。
#pragma once

#include "capture/scrollsession.h"

#include <QImage>
#include <QObject>
#include <QRect>

class QFile;
class QTimer;

class ScrollCapture : public QObject {
    Q_OBJECT

public:
    explicit ScrollCapture(QObject* parent = nullptr);

    // 首帧延迟，等框选遮罩和边框窗口消失，避免把边框截进第一帧
    static constexpr int kFirstDelayMs = 160;

    void start(const QRect& region, int firstDelayMs = kFirstDelayMs);
    void finish(); // 停止抓取并输出拼接结果
    void cancel();

    bool isRunning() const { return m_running; }

signals:
    // 每次抓到新内容后发出：缩略图 + 已截取的逻辑高度
    void previewChanged(const QImage& thumbnail, int logicalHeight);
    void finished(const QImage& image);
    void canceled();

private:
    void scheduleNext();
    void captureFrame();
    void appendThumbnail(const QImage& piece);

    // 调试导出：设置 LUMESHOT_SCROLL_DEBUG 后，把每帧画面和判定结果写入磁盘，
    // 便于用 lumeshot-replay 离线复现，不需要反复手动滚动页面。
    void openDebugDump();
    void dumpFrame(const QImage& frame);
    void writeDebugLine(const QString& line);
    void closeDebugDump(const QImage& result, const QString& status);

    QTimer* m_timer = nullptr;
    QRect m_region;
    ScrollSession m_session;
    QImage m_thumbnail; // 供侧边栏预览的缩略图，宽度固定
    bool m_running = false;

    QString m_debugDir;    // 为空表示没有开启导出
    QFile* m_debugLog = nullptr;
    int m_debugIndex = 0;  // 已导出的帧序号
};
