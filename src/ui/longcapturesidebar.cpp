#include "ui/longcapturesidebar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kPreviewWidth = 140;   // 预览区尺寸
constexpr int kPreviewHeight = 260;
} // namespace

LongCaptureSidebar::LongCaptureSidebar(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(kPreviewWidth + 32);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    m_preview = new QLabel(this);
    m_preview->setFixedSize(kPreviewWidth, kPreviewHeight);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(QStringLiteral("background: rgba(0, 0, 0, 90);"));
    layout->addWidget(m_preview);

    m_sizeLabel = new QLabel(QStringLiteral("已截取 0 px"), this);
    m_sizeLabel->setStyleSheet(QStringLiteral("color: white;"));
    m_sizeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_sizeLabel);

    auto* hint = new QLabel(QStringLiteral("请滚动页面继续截取"), this);
    hint->setStyleSheet(QStringLiteral("color: #aaaaaa;"));
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    auto* finishButton = new QPushButton(QStringLiteral("完成"), this);
    auto* cancelButton = new QPushButton(QStringLiteral("取消"), this);
    finishButton->setToolTip(QStringLiteral("Enter"));
    cancelButton->setToolTip(QStringLiteral("Esc"));
    buttonRow->addWidget(finishButton);
    buttonRow->addWidget(cancelButton);
    layout->addLayout(buttonRow);

    connect(finishButton, &QPushButton::clicked, this, &LongCaptureSidebar::finishRequested);
    connect(cancelButton, &QPushButton::clicked, this, &LongCaptureSidebar::cancelRequested);
}

void LongCaptureSidebar::setCapture(const QImage& thumbnail, int logicalHeight)
{
    if (!thumbnail.isNull()) {
        // 整张缩略图等比塞进预览区，随截取长度变长能直观看到截了多少
        m_preview->setPixmap(QPixmap::fromImage(thumbnail).scaled(
            kPreviewWidth, kPreviewHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    m_sizeLabel->setText(QStringLiteral("已截取 %1 px").arg(logicalHeight));
}

void LongCaptureSidebar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(24, 24, 24, 235));
    painter.drawRoundedRect(rect(), 8, 8);
}

void LongCaptureSidebar::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit finishRequested();
        return;
    case Qt::Key_Escape:
        emit cancelRequested();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}
