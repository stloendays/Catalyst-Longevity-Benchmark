#include "TonyBehaviorEngine.h"

#include <QRandomGenerator>
#include <QSettings>
#include <QtMath>

namespace {
constexpr auto kPrefix = "tony/life/";
}

double TonyBehaviorEngine::clamp100(double value) {
    return qBound(0.0, value, 100.0);
}

void TonyBehaviorEngine::addBondXp(int amount) {
    bondXp_ = qBound(0, bondXp_ + amount, 4900);
}

void TonyBehaviorEngine::restore(QSettings &settings) {
    bondXp_ = qBound(0, settings.value(QString(kPrefix) + "bond_xp", bondXp_).toInt(), 4900);
    health_ = clamp100(settings.value(QString(kPrefix) + "health", health_).toDouble());
    energy_ = clamp100(settings.value(QString(kPrefix) + "energy", energy_).toDouble());
    satiety_ = clamp100(settings.value(QString(kPrefix) + "satiety", satiety_).toDouble());
    warmth_ = clamp100(settings.value(QString(kPrefix) + "warmth", warmth_).toDouble());
    affection_ = clamp100(settings.value(QString(kPrefix) + "affection", affection_).toDouble());
    loneliness_ = clamp100(settings.value(QString(kPrefix) + "loneliness", loneliness_).toDouble());
    curiosity_ = clamp100(settings.value(QString(kPrefix) + "curiosity", curiosity_).toDouble());
}

void TonyBehaviorEngine::save(QSettings &settings) const {
    settings.setValue(QString(kPrefix) + "bond_xp", bondXp_);
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
}

void TonyBehaviorEngine::onPetted() {
    addBondXp(1);
    health_ = clamp100(health_ + 0.6);
    affection_ = clamp100(affection_ + 4.0);
    loneliness_ = clamp100(loneliness_ - 7.0);
    curiosity_ = clamp100(curiosity_ - 1.0);
}

void TonyBehaviorEngine::onHugged() {
    addBondXp(2);
    health_ = clamp100(health_ + 1.5);
    affection_ = clamp100(affection_ + 7.0);
    loneliness_ = clamp100(loneliness_ - 14.0);
    warmth_ = clamp100(warmth_ + 16.0);
    energy_ = clamp100(energy_ + 1.0);
}

void TonyBehaviorEngine::onConversation() {
    addBondXp(2);
    health_ = clamp100(health_ + 0.3);
    affection_ = clamp100(affection_ + 1.0);
    loneliness_ = clamp100(loneliness_ - 6.0);
    curiosity_ = clamp100(curiosity_ - 8.0);
}

void TonyBehaviorEngine::onFed() {
    addBondXp(1);
    satiety_ = clamp100(satiety_ + 28.0);
    health_ = clamp100(health_ + 1.0);
    energy_ = clamp100(energy_ + 2.0);
    warmth_ = clamp100(warmth_ + 1.0);
    loneliness_ = clamp100(loneliness_ - 2.0);
}

void TonyBehaviorEngine::onRested() {
    health_ = clamp100(health_ + 2.0);
    energy_ = clamp100(energy_ + 9.0);
    warmth_ = clamp100(warmth_ + 2.0);
    satiety_ = clamp100(satiety_ - 1.0);
}

void TonyBehaviorEngine::onDragged(bool rough) {
    health_ = clamp100(health_ - (rough ? 6.0 : 0.5));
    curiosity_ = clamp100(curiosity_ + (rough ? 6.0 : 3.0));
    energy_ = clamp100(energy_ - (rough ? 4.0 : 1.0));
    if(!rough) affection_ = clamp100(affection_ + 0.5);
}

void TonyBehaviorEngine::onPaulaMention() {
    addBondXp(1);
    health_ = clamp100(health_ + 0.4);
    affection_ = clamp100(affection_ + 2.0);
    loneliness_ = clamp100(loneliness_ - 2.0);
    curiosity_ = clamp100(curiosity_ + 4.0);
}

TonyBehaviorEngine::Impulse TonyBehaviorEngine::chooseIdleImpulse(int hour) {
    auto *rng = QRandomGenerator::global();
    const bool night = hour >= 23 || hour < 7;

    if(health_ < 38.0 && rng->bounded(100) < 88) {
        onRested();
        return Impulse::Sleep;
    }
    if(night && energy_ < 46.0 && rng->bounded(100) < 78) {
        onRested();
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

    if(health_ < 18.0) out.mood = "critical";
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
