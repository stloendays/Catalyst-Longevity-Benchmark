from pathlib import Path

p = Path("native/qt/src/researchadvisor.cpp")
text = p.read_text(encoding="utf-8")
old = '''    if (constraints.maxAdditionalHoursPerStage > 0.0) {
        target = qMin(target, latest + constraints.maxAdditionalHoursPerStage);
    }
    target = roundPracticalTime(target, constraints.minSamplingIntervalHours);
    if (target <= latest) {
        const double fallback = constraints.minSamplingIntervalHours > 0.0
            ? constraints.minSamplingIntervalHours
            : qMax(1.0, typicalStep);
        target = latest + fallback;
    }
    return target;
}'''
new = '''    const double userCap = constraints.maxAdditionalHoursPerStage > 0.0
        ? latest + constraints.maxAdditionalHoursPerStage
        : std::numeric_limits<double>::max();
    target = qMin(target, userCap);
    target = roundPracticalTime(target, constraints.minSamplingIntervalHours);
    if (target > userCap) target = userCap;
    if (target <= latest) {
        const double fallback = constraints.minSamplingIntervalHours > 0.0
            ? constraints.minSamplingIntervalHours
            : qMax(1.0, typicalStep);
        const double allowedFallback = constraints.maxAdditionalHoursPerStage > 0.0
            ? qMin(fallback, constraints.maxAdditionalHoursPerStage)
            : fallback;
        target = latest + qMax(0.1, allowedFallback);
    }
    return target;
}'''
if old not in text:
    raise RuntimeError("suggestedExtension cap pattern missing")
text = text.replace(old, new, 1)
old = '''    if (constraints.maxAdditionalHoursPerStage > 0.0) {
        bestTarget = qMin(bestTarget, latest + constraints.maxAdditionalHoursPerStage);
    }
    return roundPracticalTime(bestTarget, constraints.minSamplingIntervalHours);
}'''
new = '''    const double userCap = constraints.maxAdditionalHoursPerStage > 0.0
        ? latest + constraints.maxAdditionalHoursPerStage
        : std::numeric_limits<double>::max();
    bestTarget = qMin(bestTarget, userCap);
    bestTarget = roundPracticalTime(bestTarget, constraints.minSamplingIntervalHours);
    if (bestTarget > userCap) bestTarget = userCap;
    if (bestTarget <= latest && constraints.maxAdditionalHoursPerStage > 0.0) {
        bestTarget = latest + qMax(0.1, constraints.maxAdditionalHoursPerStage);
    }
    return bestTarget;
}'''
if old not in text:
    raise RuntimeError("reference target cap pattern missing")
text = text.replace(old, new, 1)
p.write_text(text, encoding="utf-8")
print("Planning stage cap enforcement fixed.")
