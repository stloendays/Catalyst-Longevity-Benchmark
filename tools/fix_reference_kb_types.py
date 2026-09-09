from pathlib import Path

p = Path("native/qt/src/researchadvisor.cpp")
text = p.read_text(encoding="utf-8")
replacements = {
    "score -= qMin(20, missingConditionFields(rows).size() * 5);":
        "score -= qMin(20, static_cast<int>(missingConditionFields(rows).size()) * 5);",
    "score -= qMin(24, internallyVariableFields(rows).size() * 12);":
        "score -= qMin(24, static_cast<int>(internallyVariableFields(rows).size()) * 12);",
    "const int score = qBound(55, 92 - missing.size() * 5, 90);":
        "const int score = qBound(55, 92 - static_cast<int>(missing.size()) * 5, 90);",
}
for old, new in replacements.items():
    if old not in text:
        raise RuntimeError(f"missing pattern: {old}")
    text = text.replace(old, new, 1)
p.write_text(text, encoding="utf-8")
print("Reference advice integer type fixes applied.")
