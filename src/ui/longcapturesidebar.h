// 模块 ui：长截图侧边栏——显示缩略预览、已截高度与完成/取消按钮。
#pragma once

#include <QImage>
#include <QWidget>

class QLabel;

class LongCaptureSidebar : public QWidget {
    Q_OBJECT

public:
    explicit LongCaptureSidebar(QWidget* parent = nullptr);

    // thumbnail 为拼接结果的缩略图，logicalHeight 为已截取的逻辑高度
    void setCapture(const QImage& thumbnail, int logicalHeight);

signals:
    void finishRequested();
    void cancelRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QLabel* m_preview = nullptr;
    QLabel* m_sizeLabel = nullptr;
};
