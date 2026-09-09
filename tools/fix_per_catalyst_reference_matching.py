from pathlib import Path
p = Path("native/qt/src/researchadvisor.cpp")
text = p.read_text(encoding="utf-8")
old = '''    QMap<QString, QVector<Record>> grouped;
    for (const auto& record : records) grouped[record.catalyst.trimmed()].append(record);
    const auto referenceMatches = ReferenceKnowledgeBase::matchExperimentContext(records);
    const bool hasReferenceSupport = !referenceMatches.isEmpty();
    const QString referenceBasis = ReferenceKnowledgeBase::compactMatchText(referenceMatches, 2);

    if (analysis.conditionAudit.blocksDirectRanking()) {'''
new = '''    QMap<QString, QVector<Record>> grouped;
    for (const auto& record : records) grouped[record.catalyst.trimmed()].append(record);

    if (analysis.conditionAudit.blocksDirectRanking()) {'''
if old not in text:
    raise RuntimeError("global reference match block missing")
text = text.replace(old, new, 1)
old = '''    for (const auto& summary : analysis.catalysts) {
        const auto rows = grouped.value(summary.catalyst);
        const QString conditionText = constantConditionText(rows);'''
new = '''    for (const auto& summary : analysis.catalysts) {
        const auto rows = grouped.value(summary.catalyst);
        const auto referenceMatches = ReferenceKnowledgeBase::matchExperimentContext(rows);
        const bool hasReferenceSupport = !referenceMatches.isEmpty();
        const QString referenceBasis = ReferenceKnowledgeBase::compactMatchText(referenceMatches, 2);
        const QString conditionText = constantConditionText(rows);'''
if old not in text:
    raise RuntimeError("per-catalyst loop block missing")
text = text.replace(old, new, 1)
p.write_text(text, encoding="utf-8")
print("Reference matching is now calculated per catalyst experiment context.")
