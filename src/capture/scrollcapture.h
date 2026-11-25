// 模块 capture：长截图——定时抓取固定区域，交给 ScrollSession 做重叠判定与拼接。
#pragma once

#include "capture/scrollsession.h"

#include <QImage>
#include <QObject>
#include <QRect>

class QTimer;

class ScrollCapture : public QObject {
    Q_OBJECT

public:
    explicit ScrollCapture(QObject* parent = nullptr);

    void start(const QRect& region);
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

    QTimer* m_timer = nullptr;
    QRect m_region;
    ScrollSession m_session;
    QImage m_thumbnail; // 供侧边栏预览的缩略图，宽度固定
    bool m_running = false;
};
