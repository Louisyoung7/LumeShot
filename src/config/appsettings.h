// 模块 config：用户配置的读写（QSettings），不涉及任何界面。
#pragma once

#include <QKeySequence>
#include <QString>

struct AppSettings {
    QKeySequence hotkey;
    bool autoCopy = true;
    bool autoSave = true;
    bool autostart = false;
    QString saveDirectory; // 为空时使用默认目录

    AppSettings();

    // 从 QSettings 读取；缺失的项使用默认值
    static AppSettings load();
    void save() const;

    static QKeySequence defaultHotkey();
    QString effectiveSaveDirectory() const;
};
