#pragma once

#include <QString>

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

        bool handledLocally() const { return route != Route::ServerAgent; }
    };

    Decision resolve(const QString &prompt,
                     const QString &language,
                     bool agentConnected,
                     const TonyBehaviorEngine::Snapshot &state) const;

    static QString routeName(Route route);
};
