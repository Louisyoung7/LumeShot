#include "ui/settings/settingsdialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QUrl>

#include <utility>

SettingsDialog::SettingsDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("LumeShot 设置"));
    setModal(true);
    setMinimumWidth(480);

    m_hotkeyEdit = new HotkeyEdit(this);
    m_hotkeyEdit->setKeySequence(settings.hotkey);
    m_hotkeyEdit->setMinimumWidth(220);

    m_autoCopyCheck = new QCheckBox(QStringLiteral("截图后自动复制到剪贴板"), this);
    m_autoCopyCheck->setChecked(settings.autoCopy);

    m_autoSaveCheck = new QCheckBox(QStringLiteral("截图后自动保存为 PNG"), this);
    m_autoSaveCheck->setChecked(settings.autoSave);

    m_autostartCheck = new QCheckBox(QStringLiteral("开机自动启动"), this);
    m_autostartCheck->setChecked(settings.autostart);

    m_directoryEdit = new QLineEdit(settings.effectiveSaveDirectory(), this);
    m_browseButton = new QPushButton(QStringLiteral("浏览…"), this);

    auto* directoryRow = new QHBoxLayout;
    directoryRow->addWidget(m_directoryEdit);
    directoryRow->addWidget(m_browseButton);

    m_versionLabel = new QLabel(
        QStringLiteral("当前版本 %1").arg(QCoreApplication::applicationVersion()), this);
    m_checkUpdateButton = new QPushButton(QStringLiteral("检查更新"), this);

    auto* updateRow = new QHBoxLayout;
    updateRow->addWidget(m_versionLabel, 1);
    updateRow->addWidget(m_checkUpdateButton);

    auto* form = new QFormLayout(this);
    form->addRow(QStringLiteral("全局快捷键"), m_hotkeyEdit);
    form->addRow(m_autoCopyCheck);
    form->addRow(m_autoSaveCheck);
    form->addRow(QStringLiteral("保存目录"), directoryRow);
    form->addRow(m_autostartCheck);
    form->addRow(QStringLiteral("版本"), updateRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(m_browseButton, &QPushButton::clicked, this, &SettingsDialog::browseDirectory);
    connect(m_autoSaveCheck, &QCheckBox::toggled, this, &SettingsDialog::updateDirectoryEnabled);
    connect(m_checkUpdateButton, &QPushButton::clicked, this, [this] {
        setUpdateChecking(true);
        emit checkUpdateRequested();
    });

    updateDirectoryEnabled();
    adjustSize();
}

void SettingsDialog::setHotkeyValidator(HotkeyValidator validator)
{
    m_hotkeyEdit->setValidator(std::move(validator));
}

AppSettings SettingsDialog::settings() const
{
    AppSettings result;
    result.hotkey = m_hotkeyEdit->keySequence();
    result.autoCopy = m_autoCopyCheck->isChecked();
    result.autoSave = m_autoSaveCheck->isChecked();
    result.autostart = m_autostartCheck->isChecked();
    result.saveDirectory = m_directoryEdit->text().trimmed();
    return result;
}

void SettingsDialog::accept()
{
    const QKeySequence hotkey = m_hotkeyEdit->keySequence();
    if (hotkey.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("快捷键无效"),
                             QStringLiteral("请先在输入框里按下要使用的快捷键。"));
        return;
    }

    // 只按 A、Shift+A 这类组合会抢走正常按键，全局快捷键必须带 Ctrl/Alt/Super
    const Qt::KeyboardModifiers modifiers = hotkey[0].keyboardModifiers();
    if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        QMessageBox::warning(this, QStringLiteral("快捷键无效"),
                             QStringLiteral("全局快捷键需要包含 Ctrl、Alt 或 Super 键。"));
        return;
    }

    if (!m_autoCopyCheck->isChecked() && !m_autoSaveCheck->isChecked()) {
        QMessageBox::warning(this, QStringLiteral("未选择处理方式"),
                             QStringLiteral("自动复制和自动保存至少要开启一项。"));
        return;
    }

    QDialog::accept();
}

void SettingsDialog::browseDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择自动保存目录"), m_directoryEdit->text());
    if (!directory.isEmpty())
        m_directoryEdit->setText(directory);
}

void SettingsDialog::updateDirectoryEnabled()
{
    const bool enabled = m_autoSaveCheck->isChecked();
    m_directoryEdit->setEnabled(enabled);
    m_browseButton->setEnabled(enabled);
}

void SettingsDialog::showUpdateAvailable(const QString& version, const QString& notes,
                                         const QString& releaseUrl, const QString& downloadUrl)
{
    setUpdateChecking(false);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("发现新版本"));
    box.setText(QStringLiteral("发现新版本 v%1，当前版本 v%2。")
                    .arg(version, QCoreApplication::applicationVersion()));

    const QString description = notes.trimmed();
    QString informative = description.isEmpty() ? QStringLiteral("该版本没有填写更新说明。")
                                                : description;
    if (downloadUrl.isEmpty())
        informative += QStringLiteral("\n\n该版本没有提供 deb 安装包，请到发布页手动下载。");
    box.setInformativeText(informative);

    QPushButton* installButton = nullptr;
    if (!downloadUrl.isEmpty())
        installButton = box.addButton(QStringLiteral("下载并安装"), QMessageBox::AcceptRole);
    QPushButton* openButton = nullptr;
    if (!releaseUrl.isEmpty())
        openButton = box.addButton(QStringLiteral("打开发布页"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.exec();

    if (installButton && box.clickedButton() == installButton) {
        showProgress();
        emit installUpdateRequested(downloadUrl);
    } else if (openButton && box.clickedButton() == openButton) {
        QDesktopServices::openUrl(QUrl(releaseUrl));
    }
}

void SettingsDialog::showUpToDate(const QString& version)
{
    setUpdateChecking(false);
    QMessageBox::information(this, QStringLiteral("检查更新"),
                             QStringLiteral("当前已是最新版本 v%1。").arg(version));
}

void SettingsDialog::showUpdateFailed(const QString& message)
{
    setUpdateChecking(false);
    QMessageBox::warning(this, QStringLiteral("检查更新失败"),
                         QStringLiteral("无法获取更新信息：\n%1").arg(message));
}

void SettingsDialog::showDownloadProgress(qint64 received, qint64 total)
{
    if (!m_progress)
        return;

    if (total > 0) {
        m_progress->setRange(0, 100);
        m_progress->setValue(int(received * 100 / total));
        m_progress->setLabelText(QStringLiteral("正在下载更新包… %1%")
                                     .arg(received * 100 / total));
    } else {
        m_progress->setRange(0, 0);
        m_progress->setLabelText(QStringLiteral("正在下载更新包…"));
    }
}

void SettingsDialog::showInstalling()
{
    if (!m_progress)
        return;
    m_progress->setRange(0, 0);
    m_progress->setLabelText(QStringLiteral("正在安装，请在授权窗口输入密码…"));
    m_progress->setCancelButton(nullptr);
}

void SettingsDialog::showInstallResult(bool success, const QString& message)
{
    closeProgress();
    if (success)
        QMessageBox::information(this, QStringLiteral("更新完成"), message);
    else
        QMessageBox::warning(this, QStringLiteral("安装失败"), message);
}

void SettingsDialog::showInstallFailed(const QString& message)
{
    closeProgress();
    QMessageBox::warning(this, QStringLiteral("更新失败"), message);
}

void SettingsDialog::showProgress()
{
    if (!m_progress) {
        m_progress = new QProgressDialog(this);
        m_progress->setWindowTitle(QStringLiteral("正在更新 LumeShot"));
        m_progress->setWindowModality(Qt::WindowModal);
        m_progress->setAutoClose(false);
        m_progress->setAutoReset(false);
        m_progress->setMinimumDuration(0);
        connect(m_progress, &QProgressDialog::canceled, this, [this] {
            emit updateDownloadCanceled();
            closeProgress();
        });
    }
    m_progress->setRange(0, 0);
    m_progress->setLabelText(QStringLiteral("正在下载更新包…"));
    m_progress->show();
}

void SettingsDialog::closeProgress()
{
    if (!m_progress)
        return;
    m_progress->hide();
    m_progress->deleteLater();
    m_progress = nullptr;
}

void SettingsDialog::setUpdateChecking(bool checking)
{
    m_checkUpdateButton->setEnabled(!checking);
    m_checkUpdateButton->setText(checking ? QStringLiteral("检查中…")
                                          : QStringLiteral("检查更新"));
}
