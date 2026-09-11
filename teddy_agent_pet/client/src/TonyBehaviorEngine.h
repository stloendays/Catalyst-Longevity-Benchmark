#pragma once
#include <QString>
#include <QtGlobal>

class QSettings;

class TonyBehaviorEngine {
public:
    enum class Impulse {
        None,
        Sleep,
        Shiver,
        AskHug,
        Wave,
        Walk,
        Study,
        AdjustGlasses,
        Stretch,
        Yawn,
        RemoveGlasses
    };

    struct Snapshot {
        int energy{0};
        int warmth{0};
        int affection{0};
        int loneliness{0};
        int curiosity{0};
        QString mood;
    };

    void restore(QSettings &settings);
    void save(QSettings &settings) const;
    void tick(qint64 elapsedMs, bool agentBusy, bool userNearby, int hour);

    void onPetted();
    void onHugged();
    void onConversation();
    void onDragged(bool rough);
    void onPaulaMention();

    Impulse chooseIdleImpulse(int hour);
    Snapshot snapshot() const;

private:
    static double clamp100(double value);

    double energy_{78.0};
    double warmth_{68.0};
    double affection_{62.0};
    double loneliness_{18.0};
    double curiosity_{58.0};
};
