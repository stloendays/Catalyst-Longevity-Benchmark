#include "LocalReminderManager.h"
#include "TonyBehaviorEngine.h"
#include "TonyConversationStore.h"
#include "TonyMemoryStore.h"
#include "TonyResponseRouter.h"

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
    ok &= expect(state.health == 100, "default health");
    ok &= expect(state.level == 1, "default bond level");
    ok &= expect(state.bondXp == 0, "default bond xp");
    ok &= expect(state.growthStage == QStringLiteral("pup"), "default growth stage");
    ok &= expect(state.personality == QStringLiteral("balanced"), "default personality is balanced");
    ok &= expect(state.favoriteFood == QStringLiteral("none"), "default favorite food is unknown");
    ok &= expect(state.achievements.isEmpty(), "default achievements are empty");
    ok &= expect(!state.underWeather, "default condition is healthy");
    ok &= expect(state.energy == 78, "default energy");
    ok &= expect(state.satiety == 72, "default satiety");
    ok &= expect(state.warmth == 68, "default warmth");
    ok &= expect(state.affection == 62, "default affection");
    ok &= expect(state.loneliness == 18, "default loneliness");
    ok &= expect(state.curiosity == 58, "default curiosity");
    ok &= expect(state.mood == QStringLiteral("content"), "default mood");

    engine.onDragged(true);
    state = engine.snapshot();
    ok &= expect(state.health == 94, "rough drag lowers health");

    engine.onHugged();
    state = engine.snapshot();
    ok &= expect(state.health == 96, "hug restores health");
    ok &= expect(state.energy == 75, "hug raises energy after rough drag");
    ok &= expect(state.warmth == 84, "hug raises warmth");
    ok &= expect(state.affection == 69, "hug raises affection");
    ok &= expect(state.loneliness == 4, "hug lowers loneliness");

    engine.onFed();
    state = engine.snapshot();
    ok &= expect(state.satiety == 100, "feeding raises satiety");
    ok &= expect(state.health == 97, "feeding restores a little health");
    ok &= expect(state.energy == 77, "feeding restores energy");
    ok &= expect(state.bondXp == 3, "hug and feeding accumulate bond xp");

    engine.onRested();
    state = engine.snapshot();
    ok &= expect(state.health == 99, "rest restores health");
    ok &= expect(state.energy == 86, "rest restores energy");
    ok &= expect(state.satiety == 99, "rest consumes a little satiety");
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
    ok &= expectNear(persisted.value(QStringLiteral("tony/life/satiety")).toDouble(), 71.96, 1e-6,
                     "one daytime minute lowers satiety deterministically");
    ok &= expectNear(persisted.value(QStringLiteral("tony/life/warmth")).toDouble(), 67.88, 1e-6,
                     "one daytime minute lowers warmth deterministically");
    ok &= expectNear(persisted.value(QStringLiteral("tony/life/loneliness")).toDouble(), 18.18, 1e-6,
                     "one minute away raises loneliness deterministically");
    ok &= expectNear(persisted.value(QStringLiteral("tony/life/curiosity")).toDouble(), 58.18, 1e-6,
                     "idle daytime minute raises curiosity deterministically");

    persisted.setValue(QStringLiteral("tony/life/bond_xp"), 99999);
    persisted.setValue(QStringLiteral("tony/life/health"), -50.0);
    persisted.setValue(QStringLiteral("tony/life/energy"), 250.0);
    persisted.setValue(QStringLiteral("tony/life/satiety"), 130.0);
    persisted.setValue(QStringLiteral("tony/life/warmth"), -20.0);
    persisted.setValue(QStringLiteral("tony/life/affection"), 120.0);
    persisted.setValue(QStringLiteral("tony/life/loneliness"), -4.0);
    persisted.setValue(QStringLiteral("tony/life/curiosity"), 101.0);
    persisted.sync();

    TonyBehaviorEngine restored;
    restored.restore(persisted);
    const auto clamped = restored.snapshot();
    ok &= expect(clamped.level == 99, "restore clamps bond level to 99");
    ok &= expect(clamped.bondXp == 4900, "restore clamps bond xp");
    ok &= expect(clamped.health == 0, "restore clamps health to 0");
    ok &= expect(clamped.energy == 100, "restore clamps energy to 100");
    ok &= expect(clamped.satiety == 100, "restore clamps satiety to 100");
    ok &= expect(clamped.warmth == 0, "restore clamps warmth to 0");
    ok &= expect(clamped.affection == 100, "restore clamps affection to 100");
    ok &= expect(clamped.loneliness == 0, "restore clamps loneliness to 0");
    ok &= expect(clamped.curiosity == 100, "restore clamps curiosity to 100");
    return ok;
}

bool testHungerMoodAndFeeding(const QString &settingsPath) {
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.clear();
    settings.setValue(QStringLiteral("tony/life/health"), 80.0);
    settings.setValue(QStringLiteral("tony/life/energy"), 78.0);
    settings.setValue(QStringLiteral("tony/life/satiety"), 12.0);
    settings.setValue(QStringLiteral("tony/life/warmth"), 68.0);
    settings.setValue(QStringLiteral("tony/life/affection"), 62.0);
    settings.setValue(QStringLiteral("tony/life/loneliness"), 18.0);
    settings.setValue(QStringLiteral("tony/life/curiosity"), 58.0);
    settings.sync();

    TonyBehaviorEngine engine;
    engine.restore(settings);
    bool ok = true;
    auto state = engine.snapshot();
    ok &= expect(state.mood == QStringLiteral("hungry"), "low satiety produces hungry mood");

    engine.onFed();
    state = engine.snapshot();
    ok &= expect(state.satiety == 40, "feeding adds 28 satiety");
    ok &= expect(state.health == 81, "feeding restores one health point");
    ok &= expect(state.mood == QStringLiteral("content"), "feeding clears hungry mood");
    return ok;
}

bool testGrowthFoodAndCondition(const QString &settingsPath) {
    QSettings settings(settingsPath, QSettings::IniFormat);
    bool ok = true;

    settings.clear();
    settings.setValue(QStringLiteral("tony/life/health"), 26.0);
    settings.setValue(QStringLiteral("tony/life/energy"), 60.0);
    settings.setValue(QStringLiteral("tony/life/satiety"), 15.0);
    settings.setValue(QStringLiteral("tony/life/warmth"), 60.0);
    settings.setValue(QStringLiteral("tony/life/affection"), 62.0);
    settings.setValue(QStringLiteral("tony/life/loneliness"), 18.0);
    settings.setValue(QStringLiteral("tony/life/curiosity"), 58.0);
    settings.sync();

    TonyBehaviorEngine recovering;
    recovering.restore(settings);
    auto state = recovering.snapshot();
    ok &= expect(state.underWeather, "low health plus hunger triggers under-weather condition");
    ok &= expect(state.mood == QStringLiteral("unwell"), "under-weather condition takes mood priority");

    recovering.onFed(TonyBehaviorEngine::Food::Meal);
    state = recovering.snapshot();
    ok &= expect(state.satiety == 60, "meal adds 45 satiety");
    ok &= expect(state.health == 28, "meal restores health");
    ok &= expect(state.energy == 64, "meal restores energy");
    ok &= expect(state.underWeather, "one meal does not instantly clear recovery condition");

    for(int i=0; i<15; ++i) recovering.onRested();
    state = recovering.snapshot();
    ok &= expect(state.health == 58, "repeated rest reaches recovery health threshold");
    ok &= expect(state.satiety == 45, "rest consumes a small amount of fullness");
    ok &= expect(!state.underWeather, "care condition clears only after full recovery thresholds");
    ok &= expect(state.achievements.contains(QStringLiteral("recovered")), "full recovery unlocks recovery achievement");

    TonyBehaviorEngine warmDrink;
    warmDrink.onFed(TonyBehaviorEngine::Food::WarmDrink);
    state = warmDrink.snapshot();
    ok &= expect(state.satiety == 88, "warm drink adds 16 satiety");
    ok &= expect(state.warmth == 80, "warm drink strongly restores warmth");
    ok &= expect(state.bondXp == 1, "warm drink care adds bond xp");

    settings.clear();
    settings.setValue(QStringLiteral("tony/life/bond_xp"), 200);
    settings.sync();
    TonyBehaviorEngine explorer;
    explorer.restore(settings);
    ok &= expect(explorer.snapshot().level == 5, "200 bond xp reaches level 5");
    ok &= expect(explorer.snapshot().growthStage == QStringLiteral("explorer"), "level 5 enters explorer stage");

    settings.setValue(QStringLiteral("tony/life/bond_xp"), 700);
    settings.sync();
    TonyBehaviorEngine companion;
    companion.restore(settings);
    ok &= expect(companion.snapshot().level == 15, "700 bond xp reaches level 15");
    ok &= expect(companion.snapshot().growthStage == QStringLiteral("companion"), "level 15 enters companion stage");

    settings.setValue(QStringLiteral("tony/life/bond_xp"), 1450);
    settings.sync();
    TonyBehaviorEngine veteran;
    veteran.restore(settings);
    ok &= expect(veteran.snapshot().level == 30, "1450 bond xp reaches level 30");
    ok &= expect(veteran.snapshot().growthStage == QStringLiteral("veteran"), "level 30 enters veteran stage");
    ok &= expect(veteran.snapshot().achievements.contains(QStringLiteral("veteran")),
                 "existing level-30 users derive veteran achievement on restore");

    return ok;
}

bool testPersonalityPreferencesAndAchievements(const QString &settingsPath) {
    bool ok = true;

    TonyBehaviorEngine playful;
    for(int i=0;i<5;++i) playful.onPlayed();
    auto state=playful.snapshot();
    ok &= expect(state.personality == QStringLiteral("playful"), "repeated play creates playful personality");
    ok &= expect(state.playfulnessScore == 20, "play score accumulates deterministically");
    ok &= expect(state.achievements.contains(QStringLiteral("performer")), "five play sessions unlock performer achievement");

    TonyBehaviorEngine scholar;
    for(int i=0;i<5;++i) scholar.onStudied();
    state=scholar.snapshot();
    ok &= expect(state.personality == QStringLiteral("scholar"), "repeated study creates scholar personality");
    ok &= expect(state.scholarScore == 20, "scholar score accumulates deterministically");
    ok &= expect(state.achievements.contains(QStringLiteral("scholar")), "five study sessions unlock scholar achievement");

    TonyBehaviorEngine social;
    for(int i=0;i<4;++i) social.onHugged();
    state=social.snapshot();
    ok &= expect(state.personality == QStringLiteral("social"), "repeated hugs create social personality");
    ok &= expect(state.sociabilityScore == 12, "social score accumulates from hugs");

    TonyBehaviorEngine calm;
    for(int i=0;i<4;++i) calm.onRested();
    state=calm.snapshot();
    ok &= expect(state.personality == QStringLiteral("calm"), "repeated rest creates calm personality");
    ok &= expect(state.calmScore == 8, "calm score accumulates from rest");

    TonyBehaviorEngine preference;
    for(int i=0;i<3;++i) preference.onFed(TonyBehaviorEngine::Food::WarmDrink);
    state=preference.snapshot();
    ok &= expect(state.favoriteFood == QStringLiteral("warm_drink"), "three consistent feeds establish a favorite");
    ok &= expect(state.achievements.contains(QStringLiteral("favorite_found")), "favorite food discovery unlocks achievement");
    const int bondBeforeFavoriteFeed=state.bondXp;
    preference.onFed(TonyBehaviorEngine::Food::WarmDrink);
    state=preference.snapshot();
    ok &= expect(state.bondXp == bondBeforeFavoriteFeed+2, "established favorite food gives one bonus bond xp");
    ok &= expect(state.affection == 64, "established favorite food gives affection bonus");
    for(int i=0;i<6;++i) preference.onFed(TonyBehaviorEngine::Food::WarmDrink);
    state=preference.snapshot();
    ok &= expect(state.achievements.contains(QStringLiteral("caregiver")), "ten feeds unlock caregiver achievement");

    TonyBehaviorEngine bonded;
    for(int i=0;i<5;++i) bonded.onHugged();
    state=bonded.snapshot();
    ok &= expect(state.bondXp == 10, "five hugs build ten bond xp");
    ok &= expect(state.achievements.contains(QStringLiteral("first_bond")), "ten bond xp unlocks first-bond achievement");

    QSettings persisted(settingsPath,QSettings::IniFormat);
    persisted.clear();
    preference.save(persisted);
    persisted.sync();
    TonyBehaviorEngine restored;
    restored.restore(persisted);
    state=restored.snapshot();
    ok &= expect(state.favoriteFood == QStringLiteral("warm_drink"), "favorite food persists across restore");
    ok &= expect(state.calmScore == 10, "feeding-derived calm score persists across restore");
    ok &= expect(state.achievements.contains(QStringLiteral("caregiver")), "achievement flags persist across restore");
    return ok;
}

bool testResponseRouterVitalityAndCare() {
    TonyBehaviorEngine engine;
    const auto state = engine.snapshot();
    TonyResponseRouter router;
    bool ok = true;

    const auto status = router.resolve(QStringLiteral("你怎么样"), QStringLiteral("zh"), false, state);
    ok &= expect(status.handledLocally(), "status is handled locally");
    ok &= expect(status.intent == QStringLiteral("status"), "status intent is stable");
    ok &= expect(status.reply.contains(QStringLiteral("生命 100")), "status reply contains HP");
    ok &= expect(status.reply.contains(QStringLiteral("饱食 72")), "status reply contains satiety");
    ok &= expect(status.reply.contains(QStringLiteral("Lv 1")), "status reply contains level");
    ok &= expect(status.reply.contains(QStringLiteral("幼犬")), "status reply contains growth stage");
    ok &= expect(status.reply.contains(QStringLiteral("状态：健康")), "status reply contains care condition");
    ok &= expect(status.reply.contains(QStringLiteral("性格：均衡")), "status reply contains personality");

    auto personalizedState=state;
    personalizedState.personality=QStringLiteral("playful");
    personalizedState.playfulnessScore=20;
    personalizedState.sociabilityScore=3;
    personalizedState.scholarScore=1;
    personalizedState.calmScore=2;
    personalizedState.favoriteFood=QStringLiteral("warm_drink");
    personalizedState.achievements={QStringLiteral("first_bond"),QStringLiteral("performer")};

    const auto personality = router.resolve(QStringLiteral("你什么性格"), QStringLiteral("zh"), false, personalizedState);
    ok &= expect(personality.intent == QStringLiteral("personality"), "personality question gets personality intent");
    ok &= expect(personality.reply.contains(QStringLiteral("活泼")), "personality reply uses current dominant trait");
    ok &= expect(personality.reply.contains(QStringLiteral("玩心 20")), "personality reply includes learned scores");

    const auto favorite = router.resolve(QStringLiteral("你喜欢吃什么"), QStringLiteral("zh"), false, personalizedState);
    ok &= expect(favorite.intent == QStringLiteral("favorite_food"), "favorite-food question gets dedicated intent");
    ok &= expect(favorite.reply.contains(QStringLiteral("热饮")), "favorite-food reply uses learned preference");

    const auto achievements = router.resolve(QStringLiteral("你有什么成就"), QStringLiteral("zh"), false, personalizedState);
    ok &= expect(achievements.intent == QStringLiteral("achievements"), "achievement question gets dedicated intent");
    ok &= expect(achievements.reply.contains(QStringLiteral("2 个成就")), "achievement reply reports unlocked count");

    const auto feed = router.resolve(QStringLiteral("给你零食"), QStringLiteral("zh"), false, state);
    ok &= expect(feed.handledLocally(), "explicit feeding is local");
    ok &= expect(feed.intent == QStringLiteral("feed"), "explicit feeding gets feed intent");

    const auto meal = router.resolve(QStringLiteral("给你正餐"), QStringLiteral("zh"), false, state);
    ok &= expect(meal.intent == QStringLiteral("feed_meal"), "meal phrasing gets meal intent");

    const auto warmDrink = router.resolve(QStringLiteral("给你热饮"), QStringLiteral("zh"), false, state);
    ok &= expect(warmDrink.intent == QStringLiteral("feed_warm_drink"), "warm drink phrasing gets warm-drink intent");

    const auto lockedTrick = router.resolve(QStringLiteral("表演特技"), QStringLiteral("zh"), false, state);
    ok &= expect(lockedTrick.intent == QStringLiteral("trick_locked"), "level-one trick request stays locked");

    auto advancedState = state;
    advancedState.level = 12;
    advancedState.growthStage = QStringLiteral("explorer");
    const auto advancedTrick = router.resolve(QStringLiteral("表演特技"), QStringLiteral("zh"), false, advancedState);
    ok &= expect(advancedTrick.intent == QStringLiteral("trick_victory"), "level-twelve trick request unlocks victory combo");

    const auto sleep = router.resolve(QStringLiteral("晚安"), QStringLiteral("zh"), false, state);
    ok &= expect(sleep.handledLocally(), "sleep is local");
    ok &= expect(sleep.intent == QStringLiteral("sleep"), "sleep intent remains stable");
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


bool testConversationStore() {
    QSettings().remove(QStringLiteral("tony/conversation/v1"));
    TonyConversationStore store;
    bool ok = true;

    store.append(QStringLiteral("user"), QStringLiteral("  hello   Tony  "));
    store.append(QStringLiteral("assistant"), QStringLiteral("  Hi there.  "));
    QJsonArray rows = store.entries();
    ok &= expect(rows.size() == 2, "conversation stores both sides");
    ok &= expect(rows.at(0).toObject().value(QStringLiteral("role")).toString() == QStringLiteral("user"),
                 "conversation keeps user role");
    ok &= expect(rows.at(0).toObject().value(QStringLiteral("text")).toString() == QStringLiteral("hello Tony"),
                 "conversation normalizes whitespace");
    ok &= expect(rows.at(1).toObject().value(QStringLiteral("role")).toString() == QStringLiteral("assistant"),
                 "conversation keeps assistant role");

    store.clear();
    for(int i = 0; i < 70; ++i)
        store.append(QStringLiteral("user"), QStringLiteral("message %1").arg(i));
    rows = store.entries();
    ok &= expect(rows.size() == 60, "conversation caps local history at 60 entries");
    ok &= expect(rows.first().toObject().value(QStringLiteral("text")).toString() == QStringLiteral("message 10"),
                 "conversation evicts oldest entries first");
    ok &= expect(rows.last().toObject().value(QStringLiteral("text")).toString() == QStringLiteral("message 69"),
                 "conversation keeps newest entries");

    store.clear();
    ok &= expect(store.entries().isEmpty(), "conversation clear removes local history");
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
    ok &= testHungerMoodAndFeeding(temp.filePath(QStringLiteral("hunger.ini")));
    ok &= testGrowthFoodAndCondition(temp.filePath(QStringLiteral("growth-care.ini")));
    ok &= testPersonalityPreferencesAndAchievements(temp.filePath(QStringLiteral("personality.ini")));
    ok &= testResponseRouterVitalityAndCare();
    ok &= testMemoryStore();
    ok &= testConversationStore();
    ok &= testLocalReminderManager();

    if(!ok) return 1;
    std::cout << "TonyCoreTests: PASS\n";
    return 0;
}
