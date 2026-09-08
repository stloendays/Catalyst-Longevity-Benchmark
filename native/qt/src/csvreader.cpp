#include "csvreader.h"

#include "xlsxcellrange.h"
#include "xlsxdocument.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <QTextStream>
#include <algorithm>

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

bool hasRequiredColumns(const QStringList& headers) {
    const QSet<QString> catalystAliases = {
        QStringLiteral("催化剂"), QStringLiteral("样品"), QStringLiteral("catalyst"),
        QStringLiteral("catalyst_id"), QStringLiteral("sample")};
    const QSet<QString> timeAliases = {
        QStringLiteral("时间"), QStringLiteral("tos"), QStringLiteral("time"),
        QStringLiteral("time_h"), QStringLiteral("time (h)"), QStringLiteral("tos_h")};
    const QSet<QString> performanceAliases = {
        QStringLiteral("性能"), QStringLiteral("转化率"), QStringLiteral("活性"),
        QStringLiteral("performance"), QStringLiteral("conversion"), QStringLiteral("activity")};
    return findColumn(headers, catalystAliases) >= 0
        && findColumn(headers, timeAliases) >= 0
        && findColumn(headers, performanceAliases) >= 0;
}

std::optional<double> optionalNumber(const QStringList& row, int index) {
    if (index < 0 || index >= row.size()) {
        return std::nullopt;
    }
    QString value = row[index].trimmed();
    if (value.isEmpty()) {
        return std::nullopt;
    }
    value.remove(QLatin1Char(','));
    value.replace(QStringLiteral("°C"), QString());
    value.replace(QChar(0x2103), QString());
    bool ok = false;
    const double parsed = QLocale::c().toDouble(value.trimmed(), &ok);
    return ok ? std::optional<double>(parsed) : std::nullopt;
}

QString optionalText(const QStringList& row, int index) {
    return (index >= 0 && index < row.size()) ? row[index].trimmed() : QString();
}

QVector<Record> recordsFromRows(
    const QStringList& headers,
    const QVector<QStringList>& rows,
    const QString& originLabel,
    QString* errorMessage) {
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
            *errorMessage = QStringLiteral("%1 至少需要三列：催化剂、时间、性能。支持中英文列名。")
                                .arg(originLabel);
        }
        return {};
    }

    const int temperatureColumn = findColumn(headers, {
        QStringLiteral("温度"), QStringLiteral("反应温度"), QStringLiteral("temperature"),
        QStringLiteral("temperature_c"), QStringLiteral("temp_c")});
    const int ghsvColumn = findColumn(headers, {
        QStringLiteral("ghsv"), QStringLiteral("气时空速")});
    const int whsvColumn = findColumn(headers, {
        QStringLiteral("whsv"), QStringLiteral("质量空速")});
    const int pressureColumn = findColumn(headers, {
        QStringLiteral("压力"), QStringLiteral("反应压力"), QStringLiteral("pressure"),
        QStringLiteral("pressure_bar")});
    const int feedColumn = findColumn(headers, {
        QStringLiteral("进料比"), QStringLiteral("气体比例"), QStringLiteral("feed_ratio"),
        QStringLiteral("feed ratio"), QStringLiteral("ch4_co2_ratio"), QStringLiteral("ch4:co2")});
    const int metricColumn = findColumn(headers, {
        QStringLiteral("指标"), QStringLiteral("metric")});
    const int sourceColumn = findColumn(headers, {
        QStringLiteral("数据来源"), QStringLiteral("source"), QStringLiteral("provenance")});

    QVector<Record> records;
    int rejected = 0;
    const int requiredMax = std::max({catalystColumn, timeColumn, performanceColumn});

    for (const auto& row : rows) {
        if (requiredMax >= row.size()) {
            ++rejected;
            continue;
        }

        const QString catalystName = row[catalystColumn].trimmed();
        QString timeText = row[timeColumn].trimmed();
        QString performanceText = row[performanceColumn].trimmed();
        timeText.remove(QLatin1Char(','));
        performanceText.remove(QLatin1Char(','));

        bool timeOk = false;
        bool performanceOk = false;
        const double timeHours = QLocale::c().toDouble(timeText, &timeOk);
        const double performance = QLocale::c().toDouble(performanceText, &performanceOk);
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
            *errorMessage = QStringLiteral("%1 没有读取到有效数据。请检查数值列、表头和文件内容。")
                                .arg(originLabel);
        }
        return {};
    }

    if (errorMessage) {
        *errorMessage = rejected > 0
            ? QStringLiteral("%1：已读取 %2 条记录，跳过 %3 条无效行。")
                  .arg(originLabel)
                  .arg(records.size())
                  .arg(rejected)
            : QStringLiteral("%1：已读取 %2 条记录。")
                  .arg(originLabel)
                  .arg(records.size());
    }
    return records;
}

QString variantText(const QVariant& value) {
    if (!value.isValid() || value.isNull()) {
        return QString();
    }
    return value.toString().trimmed();
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
    const QString suffix = QFileInfo(path).suffix().toCaseFolded();
    if (suffix == QStringLiteral("xlsx")) {
        return readXlsxFile(path, errorMessage);
    }
    return readDelimitedFile(path, errorMessage);
}

QVector<Record> CsvReader::readDelimitedFile(const QString& path, QString* errorMessage) {
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

    QVector<QStringList> rows;
    while (!input.atEnd()) {
        const QString line = input.readLine();
        if (!line.trimmed().isEmpty()) {
            rows.append(parseRow(line));
        }
    }

    return recordsFromRows(headers, rows, QStringLiteral("CSV"), errorMessage);
}

QVector<Record> CsvReader::readXlsxFile(const QString& path, QString* errorMessage) {
    QXlsx::Document workbook(path);
    if (!workbook.load()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取 Excel 工作簿：%1").arg(path);
        }
        return {};
    }

    const QStringList sheetNames = workbook.sheetNames();
    if (sheetNames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Excel 工作簿中没有工作表。");
        }
        return {};
    }

    QString chosenSheet;
    QStringList headers;
    QXlsx::CellRange chosenRange;
    int headerRow = 1;
    int lastColumn = 0;

    for (const QString& sheetName : sheetNames) {
        if (!workbook.selectSheet(sheetName)) {
            continue;
        }
        const QXlsx::CellRange range = workbook.dimension();
        if (!range.isValid()) {
            continue;
        }

        const int candidateHeaderRow = range.firstRow();
        const int candidateLastColumn = range.lastColumn();
        QStringList candidateHeaders;
        for (int column = 1; column <= candidateLastColumn; ++column) {
            candidateHeaders.append(variantText(workbook.read(candidateHeaderRow, column)));
        }
        if (hasRequiredColumns(candidateHeaders)) {
            chosenSheet = sheetName;
            headers = candidateHeaders;
            chosenRange = range;
            headerRow = candidateHeaderRow;
            lastColumn = candidateLastColumn;
            break;
        }
    }

    if (chosenSheet.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Excel 中未找到包含“催化剂 / 时间 / 性能”三类必需列的工作表。当前会自动扫描所有工作表的首个有效行作为表头。");
        }
        return {};
    }

    workbook.selectSheet(chosenSheet);
    QVector<QStringList> rows;
    rows.reserve(std::max(0, chosenRange.lastRow() - headerRow));
    for (int rowIndex = headerRow + 1; rowIndex <= chosenRange.lastRow(); ++rowIndex) {
        QStringList row;
        row.reserve(lastColumn);
        bool anyValue = false;
        for (int column = 1; column <= lastColumn; ++column) {
            const QString value = variantText(workbook.read(rowIndex, column));
            anyValue = anyValue || !value.isEmpty();
            row.append(value);
        }
        if (anyValue) {
            rows.append(row);
        }
    }

    return recordsFromRows(
        headers,
        rows,
        QStringLiteral("Excel 工作表“%1”").arg(chosenSheet),
        errorMessage);
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
