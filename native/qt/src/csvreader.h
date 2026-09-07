#pragma once

#include "models.h"

#include <QString>
#include <QVector>

namespace catalyst {

class CsvReader {
public:
    static QVector<Record> readFile(const QString& path, QString* errorMessage = nullptr);
    static QVector<Record> demoData();

private:
    static QStringList parseRow(const QString& line);
};

} // namespace catalyst
