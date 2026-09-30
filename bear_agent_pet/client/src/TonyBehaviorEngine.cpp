#include "TonyBehaviorEngine.h"

#include <QRandomGenerator>
#include <QSettings>
#include <QtMath>

namespace {
constexpr auto kPrefix = "tony/life/";
constexpr quint32 kAchievementFirstBond = 1u << 0;
constexpr quint32 kAchievementExplorer = 1u << 1;
constexpr quint32 kAchievementCompanion = 1u << 2;
constexpr quint32 kAchievementVeteran = 1u << 3;
constexpr quint32 kAchievementCaregiver = 1u << 4;
constexpr quint32 kAchievementPerformer = 1u << 5;
constexpr quint32 kAchievementScholar = 1u << 6;
constexpr quint32 kAchievementRecovered = 1u << 7;
constexpr quint32 kAchievementFavoriteFound = 1u << 8;
}

double TonyBehaviorEngine::clamp100(double value) {
    return qBound(0.0, value, 100.0);
}

int TonyBehaviorEngine::clampCounter(int value) {
    return qBound(0, value, 1000000);
}

void TonyBehaviorEngine::addBondXp(int amount) {
    bondXp_ = qBound(0, bondXp_ + amount, 4900);
}

QString TonyBehaviorEngine::personalityName() const {
    const int scores[4]{playfulXp_, socialXp_, scholarXp_, calmXp_};
    int best=0;
    int second=0;
    int bestIndex=0;
    for(int i=0;i<4;++i) {
        if(scores[i]>best) {
            second=best;
            best=scores[i];
            bestIndex=i;
        } else if(scores[i]>second) {
            second=scores[i];
        }
    }
    if(best<5 || best-second<3) return QStringLiteral("balanced");
    switch(bestIndex) {
    case 0: return QStringLiteral("playful");
    case 1: return QStringLiteral("social");
    case 2: return QStringLiteral("scholar");
    default: return QStringLiteral("calm");
    }
}

QString TonyBehaviorEngine::favoriteFoodName() const {
    const int total=snackCount_+mealCount_+warmDrinkCount_;
    if(total<3) return QStringLiteral("none");
    const int best=qMax(snackCount_,qMax(mealCount_,warmDrinkCount_));
    if(best<3 || best*2<=total) return QStringLiteral("mixed");
    int ties=0;
    if(snackCount_==best) ++ties;
    if(mealCount_==best) ++ties;
    if(warmDrinkCount_==best) ++ties;
    if(ties!=1) return QStringLiteral("mixed");
    if(snackCount_==best) return QStringLiteral("snack");
    if(mealCount_==best) return QStringLiteral("meal");
    return QStringLiteral("warm_drink");
}

QStringList TonyBehaviorEngine::achievementNames() const {
    QStringList out;
    if(achievementFlags_ & kAchievementFirstBond) out << QStringLiteral("first_bond");
    if(achievementFlags_ & kAchievementExplorer) out << QStringLiteral("explorer");
    if(achievementFlags_ & kAchievementCompanion) out << QStringLiteral("companion");
    if(achievementFlags_ & kAchievementVeteran) out << QStringLiteral("veteran");
    if(achievementFlags_ & kAchievementCaregiver) out << QStringLiteral("caregiver");
    if(achievementFlags_ & kAchievementPerformer) out << QStringLiteral("performer");
    if(achievementFlags_ & kAchievementScholar) out << QStringLiteral("scholar");
    if(achievementFlags_ & kAchievementRecovered) out << QStringLiteral("recovered");
    if(achievementFlags_ & kAchievementFavoriteFound) out << QStringLiteral("favorite_found");
    return out;
}

void TonyBehaviorEngine::refreshAchievements() {
    const int level=qMin(99,1+bondXp_/50);
    const int totalFeeds=snackCount_+mealCount_+warmDrinkCount_;
    if(bondXp_>=10) achievementFlags_ |= kAchievementFirstBond;
    if(level>=5) achievementFlags_ |= kAchievementExplorer;
    if(level>=15) achievementFlags_ |= kAchievementCompanion;
    if(level>=30) achievementFlags_ |= kAchievementVeteran;
    if(totalFeeds>=10) achievementFlags_ |= kAchievementCaregiver;
    if(playCount_>=5) achievementFlags_ |= kAchievementPerformer;
    if(studyCount_>=5) achievementFlags_ |= kAchievementScholar;
    if(recoveryCount_>=1) achievementFlags_ |= kAchievementRecovered;
    const QString favorite=favoriteFoodName();
    if(favorite!="none" && favorite!="mixed") achievementFlags_ |= kAchievementFavoriteFound;
}

void TonyBehaviorEngine::refreshCondition(bool countRecovery) {
    const bool wasUnderWeather=underWeather_;
    if(!underWeather_ && health_ < 30.0 && (satiety_ < 22.0 || warmth_ < 28.0))
        underWeather_ = true;

    if(underWeather_ && health_ >= 58.0 && satiety_ >= 45.0 &&
       warmth_ >= 45.0 && energy_ >= 45.0)
        underWeather_ = false;

    if(countRecovery && wasUnderWeather && !underWeather_)
        recoveryCount_=clampCounter(recoveryCount_+1);
    refreshAchievements();
}

void TonyBehaviorEngine::restore(QSettings &settings) {
    bondXp_ = qBound(0, settings.value(QString(kPrefix) + "bond_xp", bondXp_).toInt(), 4900);
    playfulXp_ = clampCounter(settings.value(QString(kPrefix) + "personality_playful", playfulXp_).toInt());
    socialXp_ = clampCounter(settings.value(QString(kPrefix) + "personality_social", socialXp_).toInt());
    scholarXp_ = clampCounter(settings.value(QString(kPrefix) + "personality_scholar", scholarXp_).toInt());
    calmXp_ = clampCounter(settings.value(QString(kPrefix) + "personality_calm", calmXp_).toInt());
    snackCount_ = clampCounter(settings.value(QString(kPrefix) + "food_snack_count", snackCount_).toInt());
    mealCount_ = clampCounter(settings.value(QString(kPrefix) + "food_meal_count", mealCount_).toInt());
    warmDrinkCount_ = clampCounter(settings.value(QString(kPrefix) + "food_warm_drink_count", warmDrinkCount_).toInt());
    playCount_ = clampCounter(settings.value(QString(kPrefix) + "play_count", playCount_).toInt());
    studyCount_ = clampCounter(settings.value(QString(kPrefix) + "study_count", studyCount_).toInt());
    recoveryCount_ = clampCounter(settings.value(QString(kPrefix) + "recovery_count", recoveryCount_).toInt());
    achievementFlags_ = settings.value(QString(kPrefix) + "achievement_flags", achievementFlags_).toUInt();
    underWeather_ = settings.value(QString(kPrefix) + "under_weather", underWeather_).toBool();
    health_ = clamp100(settings.value(QString(kPrefix) + "health", health_).toDouble());
    energy_ = clamp100(settings.value(QString(kPrefix) + "energy", energy_).toDouble());
    satiety_ = clamp100(settings.value(QString(kPrefix) + "satiety", satiety_).toDouble());
    warmth_ = clamp100(settings.value(QString(kPrefix) + "warmth", warmth_).toDouble());
    affection_ = clamp100(settings.value(QString(kPrefix) + "affection", affection_).toDouble());
    loneliness_ = clamp100(settings.value(QString(kPrefix) + "loneliness", loneliness_).toDouble());
    curiosity_ = clamp100(settings.value(QString(kPrefix) + "curiosity", curiosity_).toDouble());
    refreshCondition(false);
    refreshAchievements();
}

void TonyBehaviorEngine::save(QSettings &settings) const {
    settings.setValue(QString(kPrefix) + "bond_xp", bondXp_);
    settings.setValue(QString(kPrefix) + "personality_playful", playfulXp_);
    settings.setValue(QString(kPrefix) + "personality_social", socialXp_);
    settings.setValue(QString(kPrefix) + "personality_scholar", scholarXp_);
    settings.setValue(QString(kPrefix) + "personality_calm", calmXp_);
    settings.setValue(QString(kPrefix) + "food_snack_count", snackCount_);
    settings.setValue(QString(kPrefix) + "food_meal_count", mealCount_);
    settings.setValue(QString(kPrefix) + "food_warm_drink_count", warmDrinkCount_);
    settings.setValue(QString(kPrefix) + "play_count", playCount_);
    settings.setValue(QString(kPrefix) + "study_count", studyCount_);
    settings.setValue(QString(kPrefix) + "recovery_count", recoveryCount_);
    settings.setValue(QString(kPrefix) + "achievement_flags", achievementFlags_);
    settings.setValue(QString(kPrefix) + "under_weather", underWeather_);
    settings.setValue(QString(kPrefix) + "health", health_);
    settings.setValue(QString(kPrefix) + "energy", energy_);
    settings.setValue(QString(kPrefix) + "satiety", satiety_);
    settings.setValue(QString(kPrefix) + "warmth", warmth_);
    settings.setValue(QString(kPrefix) + "affection", affection_);
    settings.setValue(QString(kPrefix) + "loneliness", loneliness_);
    settings.setValue(QString(kPrefix) + "curiosity", curiosity_);
}

void TonyBehaviorEngine::tick(qint64 elapsedMs, bool agentBusy, bool userNearby, int hour) {
    if(elapsedMs <= 0) return;
    const double minutes = qMin(1.0, static_cast<double>(elapsedMs) / 60000.0);
    const bool night = hour >= 23 || hour < 7;

    energy_ += (agentBusy ? -0.32 : (night ? -0.13 : -0.06)) * minutes;
    satiety_ -= (night ? 0.022 : (agentBusy ? 0.055 : 0.040)) * minutes;
    if(satiety_ < 25.0) energy_ -= 0.055 * minutes;
    if(underWeather_) energy_ -= 0.080 * minutes;
    warmth_ += (night ? -0.20 : -0.12) * minutes;
    loneliness_ += (userNearby ? -0.30 : 0.18) * minutes;
    curiosity_ += (agentBusy ? -0.12 : 0.18) * minutes;
    if(userNearby) {
        affection_ += 0.025 * minutes;
        warmth_ += 0.025 * minutes;
    }

    // Health is a slow-moving resilience value rather than another activity meter.
    // It only falls when Tony is genuinely depleted/cold/lonely, and recovers
    // gradually under comfortable conditions.
    double healthDrain = 0.0;
    if(energy_ < 24.0) healthDrain += (24.0 - energy_) * 0.010;
    if(warmth_ < 28.0) healthDrain += (28.0 - warmth_) * 0.014;
    if(loneliness_ > 88.0) healthDrain += (loneliness_ - 88.0) * 0.004;
    if(satiety_ < 18.0) healthDrain += (18.0 - satiety_) * 0.008;
    if(healthDrain > 0.0) {
        health_ -= healthDrain * minutes;
    } else if(energy_ > 52.0 && satiety_ > 35.0 && warmth_ > 48.0 && loneliness_ < 65.0) {
        health_ += 0.04 * minutes;
    }

    health_ = clamp100(health_);
    energy_ = clamp100(energy_);
    satiety_ = clamp100(satiety_);
    warmth_ = clamp100(warmth_);
    affection_ = clamp100(affection_);
    loneliness_ = clamp100(loneliness_);
    curiosity_ = clamp100(curiosity_);
    refreshCondition();
}

void TonyBehaviorEngine::onPetted() {
    addBondXp(1);
    socialXp_=clampCounter(socialXp_+2);
    calmXp_=clampCounter(calmXp_+1);
    health_ = clamp100(health_ + 0.6);
    affection_ = clamp100(affection_ + 4.0);
    loneliness_ = clamp100(loneliness_ - 7.0);
    curiosity_ = clamp100(curiosity_ - 1.0);
    refreshCondition();
}

void TonyBehaviorEngine::onHugged() {
    addBondXp(2);
    socialXp_=clampCounter(socialXp_+3);
    calmXp_=clampCounter(calmXp_+1);
    health_ = clamp100(health_ + 1.5);
    affection_ = clamp100(affection_ + 7.0);
    loneliness_ = clamp100(loneliness_ - 14.0);
    warmth_ = clamp100(warmth_ + 16.0);
    energy_ = clamp100(energy_ + 1.0);
    refreshCondition();
}

void TonyBehaviorEngine::onConversation() {
    addBondXp(2);
    socialXp_=clampCounter(socialXp_+1);
    health_ = clamp100(health_ + 0.3);
    affection_ = clamp100(affection_ + 1.0);
    loneliness_ = clamp100(loneliness_ - 6.0);
    curiosity_ = clamp100(curiosity_ - 8.0);
    refreshCondition();
}

void TonyBehaviorEngine::onFed() {
    onFed(Food::Snack);
}

void TonyBehaviorEngine::onFed(Food food) {
    const QString favoriteBefore=favoriteFoodName();
    const bool preferredBefore=
        (food==Food::Snack && favoriteBefore=="snack") ||
        (food==Food::Meal && favoriteBefore=="meal") ||
        (food==Food::WarmDrink && favoriteBefore=="warm_drink");
    calmXp_=clampCounter(calmXp_+1);
    switch(food) {
    case Food::Snack:
        snackCount_=clampCounter(snackCount_+1);
        addBondXp(1);
        satiety_ = clamp100(satiety_ + 28.0);
        health_ = clamp100(health_ + 1.0);
        energy_ = clamp100(energy_ + 2.0);
        warmth_ = clamp100(warmth_ + 1.0);
        loneliness_ = clamp100(loneliness_ - 2.0);
        break;
    case Food::Meal:
        mealCount_=clampCounter(mealCount_+1);
        addBondXp(2);
        satiety_ = clamp100(satiety_ + 45.0);
        health_ = clamp100(health_ + 2.0);
        energy_ = clamp100(energy_ + 4.0);
        warmth_ = clamp100(warmth_ + 2.0);
        loneliness_ = clamp100(loneliness_ - 3.0);
        break;
    case Food::WarmDrink:
        warmDrinkCount_=clampCounter(warmDrinkCount_+1);
        addBondXp(1);
        satiety_ = clamp100(satiety_ + 16.0);
        health_ = clamp100(health_ + 2.5);
        energy_ = clamp100(energy_ + 1.0);
        warmth_ = clamp100(warmth_ + 12.0);
        loneliness_ = clamp100(loneliness_ - 2.0);
        break;
    }
    if(preferredBefore) {
        addBondXp(1);
        affection_=clamp100(affection_+2.0);
    }
    refreshCondition();
}

void TonyBehaviorEngine::applyRest(bool shapePersonality) {
    if(shapePersonality) calmXp_=clampCounter(calmXp_+2);
    health_ = clamp100(health_ + 2.0);
    energy_ = clamp100(energy_ + 9.0);
    warmth_ = clamp100(warmth_ + 2.0);
    satiety_ = clamp100(satiety_ - 1.0);
    refreshCondition();
}

void TonyBehaviorEngine::onRested() {
    applyRest(true);
}

void TonyBehaviorEngine::onPassiveRested() {
    applyRest(false);
}

void TonyBehaviorEngine::onPlayed() {
    addBondXp(1);
    playfulXp_=clampCounter(playfulXp_+4);
    playCount_=clampCounter(playCount_+1);
    energy_=clamp100(energy_-1.0);
    curiosity_=clamp100(curiosity_+2.0);
    refreshAchievements();
}

void TonyBehaviorEngine::onStudied() {
    addBondXp(1);
    scholarXp_=clampCounter(scholarXp_+4);
    studyCount_=clampCounter(studyCount_+1);
    curiosity_=clamp100(curiosity_-4.0);
    energy_=clamp100(energy_-0.5);
    refreshAchievements();
}

void TonyBehaviorEngine::onDragged(bool rough) {
    if(!rough) playfulXp_=clampCounter(playfulXp_+1);
    health_ = clamp100(health_ - (rough ? 6.0 : 0.5));
    curiosity_ = clamp100(curiosity_ + (rough ? 6.0 : 3.0));
    energy_ = clamp100(energy_ - (rough ? 4.0 : 1.0));
    if(!rough) affection_ = clamp100(affection_ + 0.5);
    refreshCondition();
}

void TonyBehaviorEngine::onPaulaMention() {
    addBondXp(1);
    socialXp_=clampCounter(socialXp_+1);
    health_ = clamp100(health_ + 0.4);
    affection_ = clamp100(affection_ + 2.0);
    loneliness_ = clamp100(loneliness_ - 2.0);
    curiosity_ = clamp100(curiosity_ + 4.0);
    refreshCondition();
}

TonyBehaviorEngine::Impulse TonyBehaviorEngine::chooseIdleImpulse(int hour) {
    auto *rng = QRandomGenerator::global();
    const bool night = hour >= 23 || hour < 7;

    if(underWeather_ && rng->bounded(100) < 72) {
        onPassiveRested();
        return Impulse::Sleep;
    }
    if(health_ < 38.0 && rng->bounded(100) < 88) {
        onPassiveRested();
        return Impulse::Sleep;
    }
    if(night && energy_ < 46.0 && rng->bounded(100) < 78) {
        onPassiveRested();
        return Impulse::Sleep;
    }
    if(warmth_ < 35.0 && rng->bounded(100) < 82) return Impulse::Shiver;
    if(satiety_ < 28.0 && rng->bounded(100) < 82) return Impulse::AskFood;
    if(loneliness_ > 56.0 && rng->bounded(100) < 76) return Impulse::AskHug;
    if(energy_ < 32.0 && rng->bounded(100) < 72) return Impulse::Yawn;
    if(curiosity_ > 74.0 && rng->bounded(100) < 70) {
        curiosity_ = clamp100(curiosity_ - 11.0);
        energy_ = clamp100(energy_ - 1.0);
        return Impulse::Study;
    }

    const QString personality=personalityName();
    if(personality=="playful" && energy_>48.0 && rng->bounded(100)<22) return Impulse::Walk;
    if(personality=="social" && rng->bounded(100)<22) return Impulse::Wave;
    if(personality=="scholar" && energy_>38.0 && rng->bounded(100)<24) return Impulse::Study;
    if(personality=="calm" && rng->bounded(100)<24) return Impulse::Stretch;

    const int r = rng->bounded(100);
    if(r < 68) return Impulse::None;
    if(r < 75) return Impulse::Stretch;
    if(r < 82) return Impulse::Wave;
    if(r < 87) return Impulse::AdjustGlasses;
    if(r < 92) return Impulse::Walk;
    if(r < 96) return Impulse::Study;
    if(r < 98) return Impulse::Yawn;
    return Impulse::RemoveGlasses;
}

TonyBehaviorEngine::Snapshot TonyBehaviorEngine::snapshot() const {
    Snapshot out;
    out.health = qRound(health_);
    out.level = qMin(99, 1 + bondXp_ / 50);
    out.bondXp = bondXp_;
    out.energy = qRound(energy_);
    out.satiety = qRound(satiety_);
    out.warmth = qRound(warmth_);
    out.affection = qRound(affection_);
    out.loneliness = qRound(loneliness_);
    out.curiosity = qRound(curiosity_);
    out.playfulnessScore = playfulXp_;
    out.sociabilityScore = socialXp_;
    out.scholarScore = scholarXp_;
    out.calmScore = calmXp_;
    out.personality = personalityName();
    out.favoriteFood = favoriteFoodName();
    out.achievements = achievementNames();
    if(out.level < 5) out.growthStage = "pup";
    else if(out.level < 15) out.growthStage = "explorer";
    else if(out.level < 30) out.growthStage = "companion";
    else out.growthStage = "veteran";
    out.underWeather = underWeather_;

    if(health_ < 18.0) out.mood = "critical";
    else if(underWeather_) out.mood = "unwell";
    else if(health_ < 35.0) out.mood = "weak";
    else if(warmth_ < 35.0) out.mood = "cold";
    else if(satiety_ < 25.0) out.mood = "hungry";
    else if(energy_ < 32.0) out.mood = "sleepy";
    else if(loneliness_ > 56.0) out.mood = "cuddly";
    else if(curiosity_ > 74.0) out.mood = "curious";
    else if(affection_ > 82.0) out.mood = "happy";
    else out.mood = "content";
    return out;
}
