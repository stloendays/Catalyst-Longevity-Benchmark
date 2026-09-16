#include "PetWindow.h"

#include <QtGlobal>

TonyBehaviorEngine::Snapshot PetWindow::autonomySnapshot() const {
    return behavior_.snapshot();
}

qint64 PetWindow::autonomyInactivityMs() const {
    return activityClock_.isValid() ? qMax<qint64>(0,activityClock_.elapsed()) : 0;
}

bool PetWindow::autonomousSurfaceAvailable() const {
    return isVisible() && !hiddenForFullscreen_ && action_==Action::Idle && !actionTimer_.isActive() &&
           !dragging_ && !mouseDown_ && agentState_==QStringLiteral("idle") &&
           !composer_.isVisible() && !bubble_.isVisible();
}

void PetWindow::performAutonomousMoment(const QString &action,
                                        const QString &emotion,
                                        int durationMs,
                                        const QString &text,
                                        int bubbleMs) {
    if(!autonomousSurfaceAvailable()) return;
    emotion_=emotion.trimmed().isEmpty() ? QStringLiteral("content") : emotion.trimmed();
    const QString wireAction=action.trimmed();
    if(!wireAction.isEmpty()) setAction(actionFromWire(wireAction),qMax(0,durationMs));
    if(!text.trimmed().isEmpty()) showBubble(text.trimmed(),qMax(1200,bubbleMs));
}

void PetWindow::showAutonomyNotice(const QString &text) {
    if(text.trimmed().isEmpty()) return;
    emotion_=QStringLiteral("content");
    showBubble(text.trimmed(),3800);
}

QSystemTrayIcon *PetWindow::trayIcon() {
    return &tray_;
}
