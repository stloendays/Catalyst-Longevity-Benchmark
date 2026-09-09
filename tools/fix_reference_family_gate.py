from pathlib import Path

p = Path("native/qt/src/referenceknowledge.cpp")
text = p.read_text(encoding="utf-8")
old = '''    for (const auto& reference : experimentReferences()) {
        int score = 25; // DRM context identified.
'''
new = '''    for (const auto& reference : experimentReferences()) {
        const int familyScore = catalystFamilyScore(records, reference.catalystFamily);
        // Do not use a Ni-family literature window for a catalyst that is not
        // identifiable as Ni-based from the imported sample name. Reaction
        // conditions alone are not enough to claim a close experimental analogue.
        if (folded(reference.catalystFamily).contains(QStringLiteral("ni")) && familyScore == 0) {
            continue;
        }

        int score = 25; // DRM context identified.
'''
if old not in text:
    raise RuntimeError("reference loop pattern missing")
text = text.replace(old, new, 1)
old = '''        if (oneToOne && reference.feed.contains(QStringLiteral("1:1"))) score += 10;
        score += catalystFamilyScore(records, reference.catalystFamily);
        score = qBound(0, score, 100);
'''
new = '''        if (oneToOne && reference.feed.contains(QStringLiteral("1:1"))) score += 10;
        score += familyScore;
        score = qBound(0, score, 100);
'''
if old not in text:
    raise RuntimeError("family score pattern missing")
text = text.replace(old, new, 1)
p.write_text(text, encoding="utf-8")
print("Catalyst-family gate added to public experiment reference matching.")
