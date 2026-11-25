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
        QImage piece;  // 仅 Appended 时有效：本帧新增的部分
        int overlap = 0; // 判定得到的重叠行数，-1 表示没有滚动
    };

    explicit ScrollSession(int maxHeight = 20000);

    void reset();
    Result addFrame(const QImage& frame);

    bool isEmpty() const { return m_pieces.isEmpty(); }
    int totalHeight() const { return m_totalHeight; }
    int consecutiveMisses() const { return m_misses; }
    qreal devicePixelRatio() const { return m_devicePixelRatio; }

    // 按追加顺序合成整张长图
    QImage compose() const;

private:
    static constexpr int kMaxConsecutiveMisses = 3;

    void updateTail(const QImage& frame, int overlap);
    Result appendWholeFrame(const QImage& frame);

    int m_maxHeight;
    QImage m_tail;            // 已拼接结果底部最多一屏的画面
    QImage m_previousFrame;   // 上一帧原始画面，用于判断是否真的在滚动
    QVector<QImage> m_pieces; // 每帧新增的行
    int m_totalHeight = 0;    // 物理像素
    int m_misses = 0;         // 连续匹配失败的帧数
    qreal m_devicePixelRatio = 1.0;
};
