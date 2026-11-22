#include "config/appsettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {
const QString kHotkeyKey = QStringLiteral("capture/hotkey");
const QString kAutoCopyKey = QStringLiteral("capture/autoCopy");
const QString kAutoSaveKey = QStringLiteral("capture/autoSave");
const QString kSaveDirectoryKey = QStringLiteral("capture/saveDirectory");
const QString kAutostartKey = QStringLiteral("startup/autostart");
} // namespace

AppSettings::AppSettings()
    : hotkey(defaultHotkey())
{
}

QKeySequence AppSettings::defaultHotkey()
{
    return QKeySequence(QStringLiteral("Ctrl+Alt+A"));
}

AppSettings AppSettings::load()
{
    const QSettings store;
    AppSettings settings;

    const QString hotkeyText =
        store.value(kHotkeyKey, settings.hotkey.toString(QKeySequence::PortableText)).toString();
    settings.hotkey = QKeySequence::fromString(hotkeyText, QKeySequence::PortableText);
    if (settings.hotkey.isEmpty())
        settings.hotkey = defaultHotkey();

    settings.autoCopy = store.value(kAutoCopyKey, true).toBool();
    settings.autoSave = store.value(kAutoSaveKey, true).toBool();
    settings.autostart = store.value(kAutostartKey, false).toBool();
    settings.saveDirectory = store.value(kSaveDirectoryKey).toString();
    return settings;
}

void AppSettings::save() const
{
    QSettings store;
    store.setValue(kHotkeyKey, hotkey.toString(QKeySequence::PortableText));
    store.setValue(kAutoCopyKey, autoCopy);
    store.setValue(kAutoSaveKey, autoSave);
    store.setValue(kAutostartKey, autostart);
    store.setValue(kSaveDirectoryKey, saveDirectory);
    store.sync();
}

QString AppSettings::effectiveSaveDirectory() const
{
    if (!saveDirectory.trimmed().isEmpty())
        return saveDirectory;

    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty())
        pictures = QDir::homePath();
    return pictures + QStringLiteral("/LumeShot");
}
