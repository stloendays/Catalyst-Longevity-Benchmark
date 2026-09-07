#include "csvreader.h"

#include <QFile>
#include <QLocale>
#include <QSet>
#include <QTextStream>

namespace catalyst {

namespace {

int findColumn(const QStringList& headers, const QSet<QString>& aliases) {
    for (qsizetype i = 0; i < headers.size(); ++i) {
        if (aliases.contains(headers[i].trimmed().toCaseFolded())) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::optional<double> optionalNumber(const QStringList& row, int index) {
    if (index < 0 || index >= row.size()) {
        return std::nullopt;
    }
    const QString value = row[index].trimmed();
    if (value.isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    const double parsed = QLocale::c().toDouble(value, &ok);
    return ok ? std::optional<double>(parsed) : std::nullopt;
}

QString optionalText(const QStringList& row, int index) {
    return (index >= 0 && index < row.size()) ? row[index].trimmed() : QString();
}

} // namespace

QStringList CsvReader::parseRow(const QString& line) {
    QStringList fields;
    QString field;
    bool quoted = false;

    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (quoted) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field.append(ch);
            }
        } else if (ch == QLatin1Char('"')) {
            quoted = true;
        } else if (ch == QLatin1Char(',')) {
            fields.append(field);
            field.clear();
        } else {
            field.append(ch);
        }
    }
    fields.append(field);
    return fields;
}

QVector<Record> CsvReader::readFile(const QString& path, QString* errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开文件：%1").arg(path);
        }
        return {};
    }

    QTextStream input(&file);
    if (input.atEnd()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CSV 文件为空。");
        }
        return {};
    }

    QString headerLine = input.readLine();
    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xFEFF)) {
        headerLine.remove(0, 1);
    }
    const QStringList headers = parseRow(headerLine);

    const int catalystColumn = findColumn(headers, {
        QStringLiteral("催化剂"), QStringLiteral("样品"), QStringLiteral("catalyst"),
        QStringLiteral("catalyst_id"), QStringLiteral("sample")});
    const int timeColumn = findColumn(headers, {
        QStringLiteral("时间"), QStringLiteral("tos"), QStringLiteral("time"),
        QStringLiteral("time_h"), QStringLiteral("time (h)"), QStringLiteral("tos_h")});
    const int performanceColumn = findColumn(headers, {
        QStringLiteral("性能"), QStringLiteral("转化率"), QStringLiteral("活性"),
        QStringLiteral("performance"), QStringLiteral("conversion"), QStringLiteral("activity")});

    if (catalystColumn < 0 || timeColumn < 0 || performanceColumn < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CSV 至少需要三列：催化剂、时间、性能。支持中英文列名。");
        }
        return {};
    }

    const int temperatureColumn = findColumn(headers, {QStringLiteral("温度"), QStringLiteral("temperature"), QStringLiteral("temperature_c")});
    const int ghsvColumn = findColumn(headers, {QStringLiteral("ghsv")});
    const int whsvColumn = findColumn(headers, {QStringLiteral("whsv")});
    const int pressureColumn = findColumn(headers, {QStringLiteral("压力"), QStringLiteral("pressure")});
    const int feedColumn = findColumn(headers, {QStringLiteral("进料比"), QStringLiteral("feed_ratio"), QStringLiteral("feed ratio")});
    const int metricColumn = findColumn(headers, {QStringLiteral("指标"), QStringLiteral("metric")});
    const int sourceColumn = findColumn(headers, {QStringLiteral("数据来源"), QStringLiteral("source"), QStringLiteral("provenance")});

    QVector<Record> records;
    int lineNumber = 1;
    int rejected = 0;
    while (!input.atEnd()) {
        const QString line = input.readLine();
        ++lineNumber;
        if (line.trimmed().isEmpty()) {
            continue;
        }
        const QStringList row = parseRow(line);
        if (qMax(catalystColumn, qMax(timeColumn, performanceColumn)) >= row.size()) {
            ++rejected;
            continue;
        }

        const QString catalystName = row[catalystColumn].trimmed();
        bool timeOk = false;
        bool performanceOk = false;
        const double timeHours = QLocale::c().toDouble(row[timeColumn].trimmed(), &timeOk);
        const double performance = QLocale::c().toDouble(row[performanceColumn].trimmed(), &performanceOk);
        if (catalystName.isEmpty() || !timeOk || !performanceOk || timeHours < 0.0) {
            ++rejected;
            continue;
        }

        Record record;
        record.catalyst = catalystName;
        record.timeHours = timeHours;
        record.performance = performance;
        record.temperatureC = optionalNumber(row, temperatureColumn);
        record.gHSV = optionalNumber(row, ghsvColumn);
        record.wHSV = optionalNumber(row, whsvColumn);
        record.pressure = optionalNumber(row, pressureColumn);
        record.feedRatio = optionalText(row, feedColumn);
        record.metric = optionalText(row, metricColumn);
        record.source = optionalText(row, sourceColumn);
        records.append(record);
    }

    if (records.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("没有读取到有效数据。请检查数值列和 CSV 编码。");
        }
        return {};
    }

    if (errorMessage) {
        *errorMessage = rejected > 0
            ? QStringLiteral("已读取 %1 条记录，跳过 %2 条无效行。").arg(records.size()).arg(rejected)
            : QStringLiteral("已读取 %1 条记录。").arg(records.size());
    }
    return records;
}

QVector<Record> CsvReader::demoData() {
    return {
        {QStringLiteral("Catalyst A"), 0.0, 82.0, 700.0, 18000.0, std::nullopt, std::nullopt, QStringLiteral("1:1"), QStringLiteral("CH4 conversion"), QStringLiteral("demo")},
        {QStringLiteral("Catalyst A"), 20.0, 70.0, 700.0, 18000.0, std::nullopt, std::nullopt, QStringLiteral("1:1"), QStringLiteral("CH4 conversion"), QStringLiteral("demo")},
        {QStringLiteral("Catalyst A"), 50.0, 58.0, 700.0, 18000.0, std::nullopt, std::nullopt, QStringLiteral("1:1"), QStringLiteral("CH4 conversion"), QStringLiteral("demo")},
        {QStringLiteral("Catalyst B"), 0.0, 76.0, 700.0, 18000.0, std::nullopt, std::nullopt, QStringLiteral("1:1"), QStringLiteral("CH4 conversion"), QStringLiteral("demo")},
        {QStringLiteral("Catalyst B"), 20.0, 72.0, 700.0, 18000.0, std::nullopt, std::nullopt, QStringLiteral("1:1"), QStringLiteral("CH4 conversion"), QStringLiteral("demo")},
        {QStringLiteral("Catalyst B"), 50.0, 69.0, 700.0, 18000.0, std::nullopt, std::nullopt, QStringLiteral("1:1"), QStringLiteral("CH4 conversion"), QStringLiteral("demo")}
    };
}

} // namespace catalyst
