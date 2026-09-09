#include "builtindatasets.h"

namespace catalyst {

namespace {

Record makeRecord(
    const QString& catalyst,
    double timeHours,
    double performance,
    std::optional<double> temperatureC,
    std::optional<double> ghsv,
    std::optional<double> pressure,
    const QString& feedRatio,
    const QString& metric,
    const QString& source) {
    Record record;
    record.catalyst = catalyst;
    record.timeHours = timeHours;
    record.performance = performance;
    record.temperatureC = temperatureC;
    record.gHSV = ghsv;
    record.pressure = pressure;
    record.feedRatio = feedRatio;
    record.metric = metric;
    record.source = source;
    return record;
}

QVector<Record> stabilityComparison() {
    const QString source = QStringLiteral("内置数据集/长期稳定性对比（模拟）");
    const QString metric = QStringLiteral("CH4 conversion (%)");
    QVector<Record> records;
    const struct Series { const char* name; double values[6]; } series[] = {
        {"Ni-CeO2", {82.0, 81.0, 80.0, 78.0, 76.0, 74.0}},
        {"Ni-Al2O3", {82.0, 79.0, 76.0, 71.0, 66.0, 61.0}},
        {"Co-Al2O3", {70.0, 67.0, 64.0, 60.0, 57.0, 53.0}}
    };
    const double times[] = {0.0, 24.0, 48.0, 96.0, 144.0, 200.0};
    for (const auto& item : series) {
        for (int i = 0; i < 6; ++i) {
            records.append(makeRecord(
                QString::fromLatin1(item.name), times[i], item.values[i],
                700.0, 18000.0, 1.0, QStringLiteral("CH4:CO2=1:1"), metric, source));
        }
    }
    return records;
}

QVector<Record> t90SamplingWindow() {
    const QString source = QStringLiteral("内置数据集/T90 区间定位（模拟）");
    const QString metric = QStringLiteral("conversion (%)");
    QVector<Record> records;
    const double times[] = {0.0, 12.0, 24.0, 48.0, 72.0, 96.0};
    const double a[] = {84.0, 83.0, 81.5, 78.5, 73.5, 69.0};
    const double b[] = {80.0, 79.5, 78.8, 77.2, 75.8, 74.9};
    for (int i = 0; i < 6; ++i) {
        records.append(makeRecord(QStringLiteral("样品-A"), times[i], a[i], 650.0, 12000.0, 1.0,
                                  QStringLiteral("反应物=1:1"), metric, source));
        records.append(makeRecord(QStringLiteral("样品-B"), times[i], b[i], 650.0, 12000.0, 1.0,
                                  QStringLiteral("反应物=1:1"), metric, source));
    }
    return records;
}

QVector<Record> longDurationLowerBound() {
    const QString source = QStringLiteral("内置数据集/长周期寿命下限（模拟）");
    const QString metric = QStringLiteral("activity index");
    QVector<Record> records;
    const double times[] = {0.0, 100.0, 250.0, 500.0, 750.0, 1000.0};
    const double a[] = {100.0, 99.4, 98.8, 97.6, 96.7, 95.9};
    const double b[] = {100.0, 98.9, 97.8, 96.1, 94.8, 93.4};
    for (int i = 0; i < 6; ++i) {
        records.append(makeRecord(QStringLiteral("稳定样品-1"), times[i], a[i], 600.0, 10000.0, 5.0,
                                  QStringLiteral("A:B=2:1"), metric, source));
        records.append(makeRecord(QStringLiteral("稳定样品-2"), times[i], b[i], 600.0, 10000.0, 5.0,
                                  QStringLiteral("A:B=2:1"), metric, source));
    }
    return records;
}

QVector<Record> conditionMismatch() {
    const QString source = QStringLiteral("内置数据集/实验条件差异检查（模拟）");
    const QString metric = QStringLiteral("conversion (%)");
    QVector<Record> records;
    const double times[] = {0.0, 24.0, 48.0, 96.0};
    const double a[] = {78.0, 76.0, 73.0, 69.0};
    const double b[] = {82.0, 81.0, 79.0, 77.0};
    for (int i = 0; i < 4; ++i) {
        records.append(makeRecord(QStringLiteral("条件样品-A"), times[i], a[i], 700.0, 15000.0, 1.0,
                                  QStringLiteral("A:B=1:1"), metric, source));
        records.append(makeRecord(QStringLiteral("条件样品-B"), times[i], b[i], 750.0, 15000.0, 1.0,
                                  QStringLiteral("A:B=1:1"), metric, source));
    }
    return records;
}

QVector<Record> dataQualityCheck() {
    const QString source = QStringLiteral("内置数据集/数据质量检查（模拟）");
    const QString metric = QStringLiteral("conversion (%)");
    QVector<Record> records;
    records.append(makeRecord(QStringLiteral("质控样品-A"), 0.0, 80.0, 680.0, 15000.0, 1.0,
                              QStringLiteral("A:B=1:1"), metric, source));
    records.append(makeRecord(QStringLiteral("质控样品-A"), 24.0, 77.0, 680.0, 15000.0, std::nullopt,
                              QStringLiteral("A:B=1:1"), metric, source));
    records.append(makeRecord(QStringLiteral("质控样品-A"), 24.0, 76.5, 680.0, 15000.0, std::nullopt,
                              QStringLiteral("A:B=1:1"), metric, source));
    records.append(makeRecord(QStringLiteral("质控样品-A"), 72.0, 69.0, 700.0, 15000.0, std::nullopt,
                              QStringLiteral("A:B=1:1"), metric, source));

    records.append(makeRecord(QStringLiteral("质控样品-B"), 0.0, 81.0, 680.0, std::nullopt, 1.0,
                              QStringLiteral("A:B=1:1"), metric, source));
    records.append(makeRecord(QStringLiteral("质控样品-B"), 24.0, 79.5, 680.0, std::nullopt, 1.0,
                              QStringLiteral("A:B=1:1"), metric, source));
    records.append(makeRecord(QStringLiteral("质控样品-B"), 72.0, 78.0, 680.0, std::nullopt, 1.0,
                              QStringLiteral("A:B=1:1"), metric, source));
    return records;
}

} // namespace

QVector<BuiltInDataset> BuiltInDatasets::all() {
    return {
        {
            QStringLiteral("stability-comparison"),
            QStringLiteral("长期稳定性对比"),
            QStringLiteral("多催化剂长期表现"),
            QStringLiteral("三种催化剂在一致实验条件下的长期性能数据，用于展示曲线、保持率、T90 与同时间对比。"),
            stabilityComparison()
        },
        {
            QStringLiteral("t90-sampling-window"),
            QStringLiteral("T90 区间定位"),
            QStringLiteral("采样点优化"),
            QStringLiteral("用于展示 T90 落在两个实测时间点之间时，软件如何保留区间语义并给出加密采样建议。"),
            t90SamplingWindow()
        },
        {
            QStringLiteral("long-duration-lower-bound"),
            QStringLiteral("长周期寿命下限"),
            QStringLiteral("长时间稳定性测试"),
            QStringLiteral("最长 1000 h 的稳定性数据，用于展示测试结束仍未达到 T90 时的寿命下限与延长实验建议。"),
            longDurationLowerBound()
        },
        {
            QStringLiteral("condition-mismatch"),
            QStringLiteral("实验条件差异检查"),
            QStringLiteral("条件一致性审核"),
            QStringLiteral("两组数据的温度不同，用于展示软件自动发现实验条件差异并停止直接排名。"),
            conditionMismatch()
        },
        {
            QStringLiteral("data-quality-check"),
            QStringLiteral("数据质量检查"),
            QStringLiteral("缺失、重复与条件变化"),
            QStringLiteral("包含重复时间点、条件缺失和同一样品温度变化，用于展示数据完整度评分与处理建议。"),
            dataQualityCheck()
        }
    };
}

} // namespace catalyst
