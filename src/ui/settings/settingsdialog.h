// 模块 ui：设置对话框——只负责收集用户输入，不直接读写配置或注册热键。
#pragma once

#include <QDialog>

#include "config/appsettings.h"
#include "ui/settings/hotkeyedit.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings& settings, QWidget* parent = nullptr);

    // 注入快捷键可用性校验（由 app 层提供，ui 层不直接注册热键）
    void setHotkeyValidator(HotkeyValidator validator);

    // 对话框确认后的配置（取消时无意义）
    AppSettings settings() const;

signals:
    // 由 app 层执行实际检查，界面只负责展示结果
    void checkUpdateRequested();

public slots:
    void showUpdateAvailable(const QString& version, const QString& notes,
                             const QString& releaseUrl);
    void showUpToDate(const QString& version);
    void showUpdateFailed(const QString& message);

protected:
    void accept() override;

private:
    void browseDirectory();
    void updateDirectoryEnabled();
    void setUpdateChecking(bool checking);

    HotkeyEdit* m_hotkeyEdit = nullptr;
    QCheckBox* m_autoCopyCheck = nullptr;
    QCheckBox* m_autoSaveCheck = nullptr;
    QCheckBox* m_autostartCheck = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_versionLabel = nullptr;
    QPushButton* m_checkUpdateButton = nullptr;
};
