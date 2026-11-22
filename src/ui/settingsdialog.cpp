#include "ui/settingsdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

#include <utility>

SettingsDialog::SettingsDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("ScreenShot 设置"));
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

    auto* form = new QFormLayout(this);
    form->addRow(QStringLiteral("全局快捷键"), m_hotkeyEdit);
    form->addRow(m_autoCopyCheck);
    form->addRow(m_autoSaveCheck);
    form->addRow(QStringLiteral("保存目录"), directoryRow);
    form->addRow(m_autostartCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(m_browseButton, &QPushButton::clicked, this, &SettingsDialog::browseDirectory);
    connect(m_autoSaveCheck, &QCheckBox::toggled, this, &SettingsDialog::updateDirectoryEnabled);

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
