#include "EmbeddedActionAssets.h"
#include "EmbeddedActionAssetsInternal.h"

namespace TonyEmbeddedActions {
QHash<QString, QVector<QPixmap>> load() {
    QHash<QString, QVector<QPixmap>> result;
    result.insert(QStringLiteral("pat"), TonyEmbeddedActionsInternal::loadPat());
    result.insert(QStringLiteral("lifted"), TonyEmbeddedActionsInternal::loadLifted());
    result.insert(QStringLiteral("landing"), TonyEmbeddedActionsInternal::loadLanding());
    result.insert(QStringLiteral("dizzy"), TonyEmbeddedActionsInternal::loadDizzy());
    result.insert(QStringLiteral("stretch"), TonyEmbeddedActionsInternal::loadStretch());
    result.insert(QStringLiteral("yawn"), TonyEmbeddedActionsInternal::loadYawn());
    result.insert(QStringLiteral("cursor_watch"), TonyEmbeddedActionsInternal::loadCursorWatch());
    return result;
}
}
