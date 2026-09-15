#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "TonyBehaviorEngine.h"

class TonyResponseRouter {
public:
    enum class Route { LocalFixed, LocalAction, ServerAgent };

    struct Decision {
        Route route{Route::ServerAgent};
        QString intent;
        QString reply;
        QString action;
        QString emotion;
        QString forwardText;
        int durationMs{2200};
        QStringList actionSequence;
        QStringList emotionSequence;
        QVector<int> sequenceDurationsMs;

        bool handledLocally() const { return route != Route::ServerAgent; }
        bool hasActionSequence() const { return !actionSequence.isEmpty(); }
    };

    Decision resolve(const QString &prompt,
                     const QString &language,
                     bool agentConnected,
                     const TonyBehaviorEngine::Snapshot &state) const;

    static QString routeName(Route route);
};
