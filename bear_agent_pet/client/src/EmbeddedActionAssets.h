#pragma once
#include <QHash>
#include <QPixmap>
#include <QString>
#include <QVector>

namespace TonyEmbeddedActions {
QHash<QString, QVector<QPixmap>> load();
}
