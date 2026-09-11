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

void TonyBehaviorEngine::restore(QSettings &settings) {
    energy_ = clamp100(settings.value(QString(kPrefix) + "energy", energy_).toDouble());
    warmth_ = clamp100(settings.value(QString(kPrefix) + "warmth", warmth_).toDouble());
    affection_ = clamp100(settings.value(QString(kPrefix) + "affection", affection_).toDouble());
    loneliness_ = clamp100(settings.value(QString(kPrefix) + "loneliness", loneliness_).toDouble());
    curiosity_ = clamp100(settings.value(QString(kPrefix) + "curiosity", curiosity_).toDouble());
}

void TonyBehaviorEngine::save(QSettings &settings) const {
    settings.setValue(QString(kPrefix) + "energy", energy_);
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
    warmth_ += (night ? -0.20 : -0.12) * minutes;
    loneliness_ += (userNearby ? -0.30 : 0.18) * minutes;
    curiosity_ += (agentBusy ? -0.12 : 0.18) * minutes;
    if(userNearby) {
        affection_ += 0.025 * minutes;
        warmth_ += 0.025 * minutes;
    }

    energy_ = clamp100(energy_);
    warmth_ = clamp100(warmth_);
    affection_ = clamp100(affection_);
    loneliness_ = clamp100(loneliness_);
    curiosity_ = clamp100(curiosity_);
}

void TonyBehaviorEngine::onPetted() {
    affection_ = clamp100(affection_ + 4.0);
    loneliness_ = clamp100(loneliness_ - 7.0);
    curiosity_ = clamp100(curiosity_ - 1.0);
}

void TonyBehaviorEngine::onHugged() {
    affection_ = clamp100(affection_ + 7.0);
    loneliness_ = clamp100(loneliness_ - 14.0);
    warmth_ = clamp100(warmth_ + 16.0);
    energy_ = clamp100(energy_ + 1.0);
}

void TonyBehaviorEngine::onConversation() {
    affection_ = clamp100(affection_ + 1.0);
    loneliness_ = clamp100(loneliness_ - 6.0);
    curiosity_ = clamp100(curiosity_ - 8.0);
}

void TonyBehaviorEngine::onDragged(bool rough) {
    curiosity_ = clamp100(curiosity_ + (rough ? 6.0 : 3.0));
    energy_ = clamp100(energy_ - (rough ? 4.0 : 1.0));
    if(!rough) affection_ = clamp100(affection_ + 0.5);
}

void TonyBehaviorEngine::onPaulaMention() {
    affection_ = clamp100(affection_ + 2.0);
    loneliness_ = clamp100(loneliness_ - 2.0);
    curiosity_ = clamp100(curiosity_ + 4.0);
}

TonyBehaviorEngine::Impulse TonyBehaviorEngine::chooseIdleImpulse(int hour) {
    auto *rng = QRandomGenerator::global();
    const bool night = hour >= 23 || hour < 7;

    if(night && energy_ < 46.0 && rng->bounded(100) < 78) {
        energy_ = clamp100(energy_ + 8.0);
        return Impulse::Sleep;
    }
    if(warmth_ < 35.0 && rng->bounded(100) < 82) return Impulse::Shiver;
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
    out.energy = qRound(energy_);
    out.warmth = qRound(warmth_);
    out.affection = qRound(affection_);
    out.loneliness = qRound(loneliness_);
    out.curiosity = qRound(curiosity_);

    if(warmth_ < 35.0) out.mood = "cold";
    else if(energy_ < 32.0) out.mood = "sleepy";
    else if(loneliness_ > 56.0) out.mood = "cuddly";
    else if(curiosity_ > 74.0) out.mood = "curious";
    else if(affection_ > 82.0) out.mood = "happy";
    else out.mood = "content";
    return out;
}
