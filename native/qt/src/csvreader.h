#pragma once

#include "models.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace catalyst {

class CsvReader {
public:
    // Historical class name retained for source compatibility. readFile now
    // accepts both delimited CSV data and native .xlsx workbooks.
    static QVector<Record> readFile(const QString& path, QString* errorMessage = nullptr);
    static QVector<Record> demoData();

private:
    static QStringList parseRow(const QString& line);
    static QVector<Record> readDelimitedFile(const QString& path, QString* errorMessage);
    static QVector<Record> readXlsxFile(const QString& path, QString* errorMessage);
};

} // namespace catalyst
