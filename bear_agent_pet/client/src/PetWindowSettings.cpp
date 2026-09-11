#include "PetWindow.h"

#include "AppLogger.h"

#include <QJsonObject>
#include <QSettings>

void PetWindow::applyUiLanguage(const QString &language) {
    const QString normalized = language.trimmed().toLower();
    const QString code = (normalized.startsWith(QStringLiteral("zh")) || normalized == QStringLiteral("cn"))
        ? QStringLiteral("zh") : QStringLiteral("en");
    QSettings().setValue(QStringLiteral("ui/language"), code);
    agent_.setLanguage(code);
    composer_.setLanguage(code);
    AppLogger::recordOperatorEvent(
        QStringLiteral("language_change"),
        {},
        QJsonObject{{QStringLiteral("language"), code}});
    showBubble(code == QStringLiteral("zh")
                   ? QStringLiteral("语言已切换为简体中文。")
                   : QStringLiteral("Language changed to English."),
               3200);
}

void PetWindow::syncOperatorLogsForUpdate(const QString &targetVersion) {
    const QStringList batchIds = AppLogger::prepareOperatorLogUploadBatches();
    if(batchIds.isEmpty()) return;

    if(!agent_.connected()) {
        qWarning().noquote() << "Operator log sync deferred because Tony is offline;"
                             << batchIds.size() << "batch(es) remain in the local outbox";
        return;
    }

    int queued = 0;
    for(const QString &batchId : batchIds) {
        const QByteArray payload = AppLogger::readOperatorLogBatch(batchId);
        if(payload.isEmpty()) {
            qWarning().noquote() << "Could not read operator log batch" << batchId.left(12);
            continue;
        }
        if(agent_.sendOperatorLogBatch(batchId, targetVersion, payload)) ++queued;
    }
    qInfo().noquote() << "Operator log update sync queued" << queued
                      << "of" << batchIds.size() << "batch(es) for Tony" << targetVersion;
}
