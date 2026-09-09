#include "referenceknowledge.h"

#include <QSet>
#include <QtMath>
#include <algorithm>

namespace catalyst {

namespace {

constexpr auto kSnapshotDate = "2026-09-09";

QString folded(const QString& value) {
    return value.trimmed().toCaseFolded();
}

bool containsAny(const QString& value, const QStringList& needles) {
    const QString text = folded(value);
    for (const auto& needle : needles) {
        if (text.contains(folded(needle))) return true;
    }
    return false;
}

std::optional<double> representativeTemperature(const QVector<Record>& records) {
    QVector<double> values;
    for (const auto& row : records) if (row.temperatureC) values.append(*row.temperatureC);
    if (values.isEmpty()) return std::nullopt;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

std::optional<double> representativeGhsv(const QVector<Record>& records) {
    QVector<double> values;
    for (const auto& row : records) if (row.gHSV) values.append(*row.gHSV);
    if (values.isEmpty()) return std::nullopt;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

std::optional<double> representativePressure(const QVector<Record>& records) {
    QVector<double> values;
    for (const auto& row : records) if (row.pressure) values.append(*row.pressure);
    if (values.isEmpty()) return std::nullopt;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

bool looksLikeDrm(const QVector<Record>& records) {
    bool methaneMetric = false;
    bool methaneFeed = false;
    bool carbonDioxideFeed = false;
    for (const auto& row : records) {
        methaneMetric = methaneMetric || containsAny(row.metric, {QStringLiteral("ch4"), QStringLiteral("methane")});
        methaneFeed = methaneFeed || containsAny(row.feedRatio, {QStringLiteral("ch4"), QStringLiteral("methane")});
        carbonDioxideFeed = carbonDioxideFeed || containsAny(row.feedRatio, {QStringLiteral("co2"), QStringLiteral("carbon dioxide")});
    }
    return (methaneFeed && carbonDioxideFeed) || (methaneMetric && carbonDioxideFeed);
}

bool hasOneToOneFeed(const QVector<Record>& records) {
    for (const auto& row : records) {
        const QString feed = folded(row.feedRatio);
        if (feed.contains(QStringLiteral("1:1")) || feed.contains(QStringLiteral("1：1"))) return true;
    }
    return false;
}

int catalystFamilyScore(const QVector<Record>& records, const QString& family) {
    if (!folded(family).contains(QStringLiteral("ni"))) return 0;
    bool hasNi = false;
    bool hasAlumina = false;
    for (const auto& row : records) {
        const QString name = folded(row.catalyst);
        hasNi = hasNi || name.contains(QStringLiteral("ni")) || name.contains(QStringLiteral("镍"));
        hasAlumina = hasAlumina || name.contains(QStringLiteral("al2o3")) || name.contains(QStringLiteral("al₂o₃"))
            || name.contains(QStringLiteral("alumina")) || name.contains(QStringLiteral("氧化铝"));
    }
    if (hasNi && hasAlumina) return 10;
    if (hasNi) return 5;
    return 0;
}

QString matchReason(
    bool drm,
    const std::optional<double>& temperature,
    const std::optional<double>& ghsv,
    const std::optional<double>& pressure,
    bool oneToOne,
    const ExperimentReference& reference) {
    QStringList parts;
    if (drm) parts.append(QStringLiteral("反应体系接近 DRM"));
    if (temperature && reference.temperatureC) {
        parts.append(QStringLiteral("温度差 %1 ℃")
            .arg(QString::number(qAbs(*temperature - *reference.temperatureC), 'g', 4)));
    }
    if (ghsv && reference.ghsv) {
        parts.append(QStringLiteral("GHSV %1 vs %2")
            .arg(QString::number(*ghsv, 'g', 6), QString::number(*reference.ghsv, 'g', 6)));
    }
    if (pressure && reference.pressureBar) {
        parts.append(QStringLiteral("压力 %1 vs %2 bar")
            .arg(QString::number(*pressure, 'g', 5), QString::number(*reference.pressureBar, 'g', 5)));
    }
    if (oneToOne && reference.feed.contains(QStringLiteral("1:1"))) parts.append(QStringLiteral("进料比例接近 1:1"));
    return parts.join(QStringLiteral("；"));
}

} // namespace

QVector<ReferenceEntry> ReferenceKnowledgeBase::entries() {
    return {
        {ReferenceKind::Compound, QStringLiteral("PubChem"), QStringLiteral("甲烷 / Methane"),
         QStringLiteral("CID 297"), QStringLiteral("CH4；分子量 16.043 g/mol"),
         QStringLiteral("DRM 进料组分身份与分子量核对"),
         QStringLiteral("https://pubchem.ncbi.nlm.nih.gov/compound/297"), QString::fromLatin1(kSnapshotDate)},
        {ReferenceKind::Compound, QStringLiteral("PubChem"), QStringLiteral("二氧化碳 / Carbon dioxide"),
         QStringLiteral("CID 280"), QStringLiteral("CO2；分子量 44.009 g/mol"),
         QStringLiteral("DRM 进料组分身份与分子量核对"),
         QStringLiteral("https://pubchem.ncbi.nlm.nih.gov/compound/280"), QString::fromLatin1(kSnapshotDate)},
        {ReferenceKind::Compound, QStringLiteral("PubChem"), QStringLiteral("氢气 / Hydrogen"),
         QStringLiteral("CID 783"), QStringLiteral("H2；分子量 2.016 g/mol"),
         QStringLiteral("产物组分身份与分子量核对"),
         QStringLiteral("https://pubchem.ncbi.nlm.nih.gov/compound/783"), QString::fromLatin1(kSnapshotDate)},
        {ReferenceKind::Compound, QStringLiteral("PubChem"), QStringLiteral("一氧化碳 / Carbon monoxide"),
         QStringLiteral("CID 281"), QStringLiteral("CO；分子量 28.010 g/mol"),
         QStringLiteral("产物组分身份与分子量核对"),
         QStringLiteral("https://pubchem.ncbi.nlm.nih.gov/compound/281"), QString::fromLatin1(kSnapshotDate)},
        {ReferenceKind::Compound, QStringLiteral("PubChem"), QStringLiteral("水 / Water"),
         QStringLiteral("CID 962"), QStringLiteral("H2O；分子量 18.015 g/mol"),
         QStringLiteral("副反应/含蒸汽体系组分核对"),
         QStringLiteral("https://pubchem.ncbi.nlm.nih.gov/compound/962"), QString::fromLatin1(kSnapshotDate)},
        {ReferenceKind::Literature, QStringLiteral("公开论文 / DOI"),
         QStringLiteral("Zhou et al., ChemCatChem (2015)"), QStringLiteral("10.1002/cctc.201500379"),
         QStringLiteral("Ni/Al2O3 DRM；700 ℃；CH4/CO2=1:1；长期测试约 100 h"),
         QStringLiteral("用于校验同类 DRM 稳定性测试时长是否处于已有公开研究量级"),
         QStringLiteral("https://doi.org/10.1002/cctc.201500379"), QString::fromLatin1(kSnapshotDate)},
        {ReferenceKind::Literature, QStringLiteral("公开论文 / DOI"),
         QStringLiteral("He et al., Processes (2021)"), QStringLiteral("10.3390/pr9040706"),
         QStringLiteral("Ni/Al2O3 DRM；700 ℃；0.1 MPa；GHSV 24000 mL g^-1 h^-1；50 h 稳定性筛选，并报告 200 h 长周期测试"),
         QStringLiteral("用于给同类 DRM 的筛选/长周期测试建议提供公开实验窗口参照"),
         QStringLiteral("https://doi.org/10.3390/pr9040706"), QString::fromLatin1(kSnapshotDate)}
    };
}

QVector<ExperimentReference> ReferenceKnowledgeBase::experimentReferences() {
    return {
        {
            QStringLiteral("zhou2015-100h"),
            QStringLiteral("Zhou et al., ChemCatChem 2015"),
            QStringLiteral("10.1002/cctc.201500379"),
            QStringLiteral("dry reforming of methane"),
            QStringLiteral("Ni/Al2O3 / NiAl2O4-derived Ni"),
            700.0,
            std::nullopt,
            std::nullopt,
            QStringLiteral("CH4:CO2=1:1"),
            100.0,
            QStringLiteral("长期稳定性测试量级"),
            QStringLiteral("https://doi.org/10.1002/cctc.201500379")
        },
        {
            QStringLiteral("he2021-50h"),
            QStringLiteral("He et al., Processes 2021"),
            QStringLiteral("10.3390/pr9040706"),
            QStringLiteral("dry reforming of methane"),
            QStringLiteral("Ni/Al2O3"),
            700.0,
            24000.0,
            1.0,
            QStringLiteral("DRM；公开文献 50 h 稳定性筛选"),
            50.0,
            QStringLiteral("阶段性稳定性筛选"),
            QStringLiteral("https://doi.org/10.3390/pr9040706")
        },
        {
            QStringLiteral("he2021-200h"),
            QStringLiteral("He et al., Processes 2021"),
            QStringLiteral("10.3390/pr9040706"),
            QStringLiteral("dry reforming of methane"),
            QStringLiteral("Ni/Al2O3-750"),
            700.0,
            24000.0,
            1.0,
            QStringLiteral("CH4:CO2:Ar=1:1:3"),
            200.0,
            QStringLiteral("长周期耐久性验证"),
            QStringLiteral("https://doi.org/10.3390/pr9040706")
        }
    };
}

QVector<ReferenceMatch> ReferenceKnowledgeBase::matchExperimentContext(const QVector<Record>& records) {
    QVector<ReferenceMatch> matches;
    if (records.isEmpty()) return matches;

    const bool drm = looksLikeDrm(records);
    if (!drm) return matches;

    const auto temperature = representativeTemperature(records);
    const auto ghsv = representativeGhsv(records);
    const auto pressure = representativePressure(records);
    const bool oneToOne = hasOneToOneFeed(records);

    for (const auto& reference : experimentReferences()) {
        int score = 25; // DRM context identified.
        if (temperature && reference.temperatureC) {
            const double delta = qAbs(*temperature - *reference.temperatureC);
            if (delta <= 25.0) score += 25;
            else if (delta <= 75.0) score += 12;
        }
        if (ghsv && reference.ghsv && *ghsv > 0.0 && *reference.ghsv > 0.0) {
            const double ratio = *ghsv / *reference.ghsv;
            if (ratio >= 0.75 && ratio <= 1.33) score += 20;
            else if (ratio >= 0.5 && ratio <= 2.0) score += 10;
        }
        if (pressure && reference.pressureBar) {
            const double delta = qAbs(*pressure - *reference.pressureBar);
            if (delta <= 0.25) score += 10;
            else if (delta <= 1.0) score += 5;
        }
        if (oneToOne && reference.feed.contains(QStringLiteral("1:1"))) score += 10;
        score += catalystFamilyScore(records, reference.catalystFamily);
        score = qBound(0, score, 100);

        if (score >= 45) {
            matches.append({reference, score,
                matchReason(drm, temperature, ghsv, pressure, oneToOne, reference)});
        }
    }

    std::sort(matches.begin(), matches.end(), [](const ReferenceMatch& a, const ReferenceMatch& b) {
        if (a.relevanceScore != b.relevanceScore) return a.relevanceScore > b.relevanceScore;
        return a.reference.durationHours < b.reference.durationHours;
    });
    return matches;
}

QString ReferenceKnowledgeBase::compactMatchText(const QVector<ReferenceMatch>& matches, int limit) {
    if (matches.isEmpty()) return QStringLiteral("无足够接近的内置公开实验参照，建议仅按当前数据和实验约束制定计划。");
    QStringList parts;
    const int count = qMin(limit, static_cast<int>(matches.size()));
    for (int i = 0; i < count; ++i) {
        const auto& match = matches[i];
        parts.append(QStringLiteral("%1（%2，相关度 %3/100）")
            .arg(match.reference.citation, match.reference.doi)
            .arg(match.relevanceScore));
    }
    return parts.join(QStringLiteral("；"));
}

} // namespace catalyst
