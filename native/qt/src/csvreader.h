#pragma once

#include "models.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace catalyst {

class CsvReader {
public:
    // 类名暂时保留，避免影响现有调用；目前也支持读取 xlsx 文件。
    static QVector<Record> readFile(const QString& path, QString* errorMessage = nullptr);
    static QVector<Record> demoData();

private:
    static QStringList parseRow(const QString& line);
    static QVector<Record> readDelimitedFile(const QString& path, QString* errorMessage);
    static QVector<Record> readXlsxFile(const QString& path, QString* errorMessage);
};

} // catalyst 命名空间
