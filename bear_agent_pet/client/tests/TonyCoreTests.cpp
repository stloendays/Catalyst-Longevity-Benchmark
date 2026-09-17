#include "LocalReminderManager.h"
#include "TonyBehaviorEngine.h"
#include "TonyMemoryStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>

namespace {

bool expect(bool condition, const char *message) {
    if(condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

bool expectNear(double actual, double expected, double tolerance, const char *message) {
    if(std::abs(actual - expected) <= tolerance) return true;
    std::cerr << "FAIL: " << message << " actual=" << actual << " expected=" << expected << '\n';
    return false;
}

bool testBehaviorDefaultsAndInteractions() {
    TonyBehaviorEngine engine;
    auto state = engine.snapshot();
    bool ok = true;
    ok &= expect(state.energy == 78, "default energy");
    ok &= expect(state.warmth == 68, "default warmth");
    ok &= expect(state.affection == 62, "default affection");
    ok &= expect(state.loneliness == 18, "default loneliness");
    ok &= expect(state.curiosity == 58, "default curiosity");
    ok &= expect(state.mood == QStringLiteral("content"), "default mood");

    engine.onHugged();
    state = engine.snapshot();
    ok &= expect(state.energy == 79, "hug raises energy");
    ok &= expect(state.warmth == 84, "hug raises warmth");
    ok &= expect(state.affection == 69, "hug raises affection");
    ok &= expect(state.loneliness == 4, "hug lowers loneliness");
    return ok;
}

bool testBehaviorTickAndClamp(const QString &settingsPath) {
    bool ok = true;
    TonyBehaviorEngine engine;
    engine.tick(60000, false, false, 12);

    QSettings persisted(settingsPath, QSettings::IniFormat);
    persisted.clear();
    engine.save(persisted);
    persisted.sync();

    ok &= expectNear(persisted.value(QStringLiteral("tony/life/energy")).toDouble(), 77.94, 1e-6,
                     "one daytime minute lowers energy deterministically");
    ok &= expectNear(persisted.value(QStringLiteral("tony/life/warmth")).toDouble(), 67.88, 1e-6,
                     "one daytime minute lowers warmth deterministically");
    ok &= expectNear(persisted.value(QStringLiteral("tony/life/loneliness")).toDouble(), 18.18, 1e-6,
                     "one minute away raises loneliness deterministically");
    ok &= expectNear(persisted.value(QStringLiteral("tony/life/curiosity")).toDouble(), 58.18, 1e-6,
                     "idle daytime minute raises curiosity deterministically");

    persisted.setValue(QStringLiteral("tony/life/energy"), 250.0);
    persisted.setValue(QStringLiteral("tony/life/warmth"), -20.0);
    persisted.setValue(QStringLiteral("tony/life/affection"), 120.0);
    persisted.setValue(QStringLiteral("tony/life/loneliness"), -4.0);
    persisted.setValue(QStringLiteral("tony/life/curiosity"), 101.0);
    persisted.sync();

    TonyBehaviorEngine restored;
    restored.restore(persisted);
    const auto clamped = restored.snapshot();
    ok &= expect(clamped.energy == 100, "restore clamps energy to 100");
    ok &= expect(clamped.warmth == 0, "restore clamps warmth to 0");
    ok &= expect(clamped.affection == 100, "restore clamps affection to 100");
    ok &= expect(clamped.loneliness == 0, "restore clamps loneliness to 0");
    ok &= expect(clamped.curiosity == 100, "restore clamps curiosity to 100");
    return ok;
}

bool testMemoryStore() {
    auto &memory = TonyMemoryStore::instance();
    memory.clear();
    bool ok = true;

    ok &= expect(memory.remember(QStringLiteral("  Likes chemistry!!!  ")), "remember accepts a fact");
    ok &= expect(memory.remember(QStringLiteral("likes chemistry.")), "remember accepts a duplicate request");
    auto facts = memory.facts();
    ok &= expect(facts.size() == 1, "memory deduplicates case-insensitively");
    ok &= expect(facts.value(0) == QStringLiteral("Likes chemistry"), "memory normalizes trailing punctuation");

    memory.clear();
    for(int i=0;i<30;++i)
        memory.remember(QStringLiteral("fact %1").arg(i));
    facts = memory.facts();
    ok &= expect(facts.size() == 24, "memory retains at most 24 explicit facts");
    ok &= expect(facts.first() == QStringLiteral("fact 6"), "memory evicts oldest facts first");
    ok &= expect(facts.last() == QStringLiteral("fact 29"), "memory keeps newest facts");

    ok &= expect(memory.forgetMatching(QStringLiteral("FACT 29")), "forgetMatching is case-insensitive");
    ok &= expect(!memory.facts().contains(QStringLiteral("fact 29")), "forgotten fact is removed");
    ok &= expect(!memory.forgetMatching(QStringLiteral("does-not-exist")), "forgetMatching reports no match");

    memory.clear();
    ok &= expect(memory.facts().isEmpty(), "clear removes all explicit facts");
    return ok;
}

bool testLocalReminderManager() {
    QSettings().remove(QStringLiteral("tony/reminders/v1"));
    bool ok = true;
    const QDateTime due = QDateTime::currentDateTimeUtc().addSecs(3600);

    QString createdId;
    {
        LocalReminderManager manager;
        createdId = manager.createReminder(
            QStringLiteral("  Chemistry break  "),
            QStringLiteral("  Stretch and drink water.  "),
            due);
        ok &= expect(!createdId.isEmpty(), "reminder gets a stable id");
        ok &= expect(manager.count() == 1, "reminder is retained in memory");
        const QJsonArray items = manager.reminders();
        ok &= expect(items.size() == 1, "reminder is exposed through JSON");
        const QJsonObject item = items.first().toObject();
        ok &= expect(item.value(QStringLiteral("title")).toString() == QStringLiteral("Chemistry break"),
                     "reminder title is normalized");
        ok &= expect(item.value(QStringLiteral("text")).toString() == QStringLiteral("Stretch and drink water."),
                     "reminder text is normalized");
    }

    {
        LocalReminderManager restored;
        ok &= expect(restored.count() == 1, "reminder survives manager recreation");
        const QJsonArray items = restored.reminders();
        ok &= expect(items.first().toObject().value(QStringLiteral("id")).toString() == createdId,
                     "persisted reminder keeps its id");
        ok &= expect(restored.cancelReminder(createdId), "reminder can be cancelled");
        ok &= expect(restored.count() == 0, "cancelled reminder is removed");
        ok &= expect(!restored.cancelReminder(createdId), "cancelling twice reports no match");
    }

    QSettings().remove(QStringLiteral("tony/reminders/v1"));
    return ok;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("TonyDesktopPetTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TonyCoreTests"));

    QTemporaryDir temp;
    if(!temp.isValid()) {
        std::cerr << "FAIL: could not create temporary settings directory\n";
        return 1;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());

    bool ok = true;
    ok &= testBehaviorDefaultsAndInteractions();
    ok &= testBehaviorTickAndClamp(temp.filePath(QStringLiteral("behavior.ini")));
    ok &= testMemoryStore();
    ok &= testLocalReminderManager();

    if(!ok) return 1;
    std::cout << "TonyCoreTests: PASS\n";
    return 0;
}
