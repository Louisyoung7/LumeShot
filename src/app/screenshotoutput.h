// 模块 app：截图输出——按设置复制到剪贴板、保存为 PNG，并生成结果提示。
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

    void deliver(const QPixmap& capture, const QString& title);

signals:
    void messageReady(const QString& title, const QString& message);

private:
    AppSettings m_settings;
};
