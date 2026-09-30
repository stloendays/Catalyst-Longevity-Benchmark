#pragma once
#include <QString>
#include <QStringList>
#include <QtGlobal>

class QSettings;

class TonyBehaviorEngine {
public:
    enum class Food {
        Snack,
        Meal,
        WarmDrink
    };

    enum class Impulse {
        None,
        Sleep,
        Shiver,
        AskHug,
        AskFood,
        Wave,
        Walk,
        Study,
        AdjustGlasses,
        Stretch,
        Yawn,
        RemoveGlasses
    };

    struct Snapshot {
        int health{0};
        int level{1};
        int bondXp{0};
        int energy{0};
        int satiety{0};
        int warmth{0};
        int affection{0};
        int loneliness{0};
        int curiosity{0};
        int playfulnessScore{0};
        int sociabilityScore{0};
        int scholarScore{0};
        int calmScore{0};
        QString growthStage;
        QString personality;
        QString favoriteFood;
        QStringList achievements;
        bool underWeather{false};
        QString mood;
    };

    void restore(QSettings &settings);
    void save(QSettings &settings) const;
    void tick(qint64 elapsedMs, bool agentBusy, bool userNearby, int hour);

    void onPetted();
    void onHugged();
    void onConversation();
    void onFed();
    void onFed(Food food);
    void onRested();
    void onPassiveRested();
    void onPlayed();
    void onStudied();
    void onDragged(bool rough);
    void onPaulaMention();

    Impulse chooseIdleImpulse(int hour);
    Snapshot snapshot() const;

private:
    static double clamp100(double value);
    static int clampCounter(int value);
    void addBondXp(int amount);
    void applyRest(bool shapePersonality);
    void refreshCondition(bool countRecovery=true);
    void refreshAchievements();
    QString personalityName() const;
    QString favoriteFoodName() const;
    QStringList achievementNames() const;

    int bondXp_{0};
    int playfulXp_{0};
    int socialXp_{0};
    int scholarXp_{0};
    int calmXp_{0};
    int snackCount_{0};
    int mealCount_{0};
    int warmDrinkCount_{0};
    int playCount_{0};
    int studyCount_{0};
    int recoveryCount_{0};
    quint32 achievementFlags_{0};
    bool underWeather_{false};
    double health_{100.0};
    double energy_{78.0};
    double satiety_{72.0};
    double warmth_{68.0};
    double affection_{62.0};
    double loneliness_{18.0};
    double curiosity_{58.0};
};
