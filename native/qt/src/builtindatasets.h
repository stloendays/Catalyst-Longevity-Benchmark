#pragma once

#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

struct BuiltInDataset {
    QString id;
    QString name;
    QString scenario;
    QString description;
    QVector<Record> records;
};

class BuiltInDatasets {
public:
    static QVector<BuiltInDataset> all();
};

} // namespace catalyst
