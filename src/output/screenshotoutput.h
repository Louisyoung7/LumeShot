// 模块 output：截图输出——按设置复制到剪贴板、保存为 PNG，并生成结果提示。
#pragma once

#include <QObject>
#include <QString>

#include "config/appsettings.h"

class QPixmap;

class ScreenshotOutput : public QObject {
    Q_OBJECT

public:
    explicit ScreenshotOutput(QObject* parent = nullptr);

    void setSettings(const AppSettings& settings);

    // directoryOverride 非空时只保存到该目录，不改变全局设置，且不受 autoSave 开关限制
    void deliver(const QPixmap& capture, const QString& title,
                 const QString& directoryOverride = QString());

signals:
    void messageReady(const QString& title, const QString& message);

private:
    AppSettings m_settings;
};
