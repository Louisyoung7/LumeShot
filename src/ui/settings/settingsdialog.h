// 模块 ui：设置对话框——只负责收集用户输入，不直接读写配置或注册热键。
#pragma once

#include <QDialog>

#include "config/appsettings.h"
#include "ui/settings/hotkeyedit.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressDialog;
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
    void installUpdateRequested(const QString& downloadUrl);
    void updateDownloadCanceled();

public slots:
    void showUpdateAvailable(const QString& version, const QString& notes,
                             const QString& releaseUrl, const QString& downloadUrl);
    void showUpToDate(const QString& version);
    void showUpdateFailed(const QString& message);
    void showDownloadProgress(qint64 received, qint64 total);
    void showInstalling();
    void showInstallResult(bool success, const QString& message);
    void showInstallFailed(const QString& message);

protected:
    void accept() override;

private:
    void browseDirectory();
    void updateDirectoryEnabled();
    void setUpdateChecking(bool checking);
    void showProgress();
    void closeProgress();

    HotkeyEdit* m_hotkeyEdit = nullptr;
    QCheckBox* m_autoCopyCheck = nullptr;
    QCheckBox* m_autoSaveCheck = nullptr;
    QCheckBox* m_autostartCheck = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_versionLabel = nullptr;
    QPushButton* m_checkUpdateButton = nullptr;
    QProgressDialog* m_progress = nullptr;
};
