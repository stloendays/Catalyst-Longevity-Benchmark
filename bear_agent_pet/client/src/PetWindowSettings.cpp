#include "PetWindow.h"

#include <QSettings>

void PetWindow::applyUiLanguage(const QString &language) {
    const QString normalized = language.trimmed().toLower();
    const QString code = (normalized.startsWith(QStringLiteral("zh")) || normalized == QStringLiteral("cn"))
        ? QStringLiteral("zh") : QStringLiteral("en");
    QSettings().setValue(QStringLiteral("ui/language"), code);
    agent_.setLanguage(code);
    composer_.setLanguage(code);
    showBubble(code == QStringLiteral("zh")
                   ? QStringLiteral("语言已切换为简体中文。")
                   : QStringLiteral("Language changed to English."),
               3200);
}
