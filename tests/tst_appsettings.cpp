#include <QtTest>

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "config/appsettings.h"

class TestAppSettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void defaultsWhenEmpty();
    void roundTripsValues();
    void invalidHotkeyFallsBackToDefault();
    void effectiveDirectory();

private:
    QTemporaryDir m_settingsDir;
};

void TestAppSettings::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("LumeShotTest"));
    QCoreApplication::setApplicationName(QStringLiteral("LumeShotTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(m_settingsDir.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

void TestAppSettings::init()
{
    QSettings store;
    store.clear();
    store.sync();
}

void TestAppSettings::defaultsWhenEmpty()
{
    const AppSettings settings = AppSettings::load();

    QCOMPARE(settings.hotkey.toString(), AppSettings::defaultHotkey().toString());
    QVERIFY(settings.autoCopy);
    QVERIFY(settings.autoSave);
    QVERIFY(!settings.autostart);
    QVERIFY(settings.saveDirectory.isEmpty());
}

void TestAppSettings::roundTripsValues()
{
    AppSettings original;
    original.hotkey = QKeySequence(QStringLiteral("Ctrl+Shift+F9"));
    original.autoCopy = false;
    original.autoSave = true;
    original.autostart = true;
    original.saveDirectory = QStringLiteral("/tmp/lumeshot-test");
    original.save();

    const AppSettings loaded = AppSettings::load();
    QCOMPARE(loaded.hotkey.toString(), original.hotkey.toString());
    QCOMPARE(loaded.autoCopy, original.autoCopy);
    QCOMPARE(loaded.autoSave, original.autoSave);
    QCOMPARE(loaded.autostart, original.autostart);
    QCOMPARE(loaded.saveDirectory, original.saveDirectory);
}

void TestAppSettings::invalidHotkeyFallsBackToDefault()
{
    QSettings store;
    store.setValue(QStringLiteral("capture/hotkey"), QString());
    store.sync();

    QCOMPARE(AppSettings::load().hotkey.toString(),
             AppSettings::defaultHotkey().toString());
}

void TestAppSettings::effectiveDirectory()
{
    AppSettings settings;
    settings.saveDirectory = QStringLiteral("/tmp/custom");
    QCOMPARE(settings.effectiveSaveDirectory(), QStringLiteral("/tmp/custom"));

    settings.saveDirectory = QStringLiteral("   ");
    const QString fallback = settings.effectiveSaveDirectory();
    QVERIFY(!fallback.isEmpty());
    QVERIFY(fallback.endsWith(QStringLiteral("/LumeShot")));
}

QTEST_GUILESS_MAIN(TestAppSettings)

#include "tst_appsettings.moc"
