// 模块 capture：长截图会话的状态机——逐帧判定追加、丢弃或忽略，并合成结果。
// 不依赖计时器、抓屏和界面，便于单元测试。
#pragma once

#include <QImage>
#include <QVector>

class ScrollSession {
public:
    enum class Outcome {
        Ignored,     // 空帧，或尺寸与首帧不一致
        NoScroll,    // 画面没有滚动
        NoMatch,     // 重叠匹配不可靠，应丢弃该帧
        HeightLimit, // 追加后超过高度上限
        Appended,    // 已追加新内容
    };

    struct Result {
        Outcome outcome = Outcome::Ignored;
        QImage piece; // 仅 Appended 时有效：本帧新增的部分
    };

    explicit ScrollSession(int maxHeight = 20000);

    void reset();
    Result addFrame(const QImage& frame);

    bool isEmpty() const { return m_pieces.isEmpty(); }
    int totalHeight() const { return m_totalHeight; }
    qreal devicePixelRatio() const { return m_devicePixelRatio; }

    // 按追加顺序合成整张长图
    QImage compose() const;

private:
    void updateTail(const QImage& frame, int overlap);

    int m_maxHeight;
    QImage m_tail;            // 已拼接结果底部最多一屏的画面
    QVector<QImage> m_pieces; // 每帧新增的行
    int m_totalHeight = 0;    // 物理像素
    qreal m_devicePixelRatio = 1.0;
};
