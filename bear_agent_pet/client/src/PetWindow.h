#pragma once
#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include "AgentClient.h"
#include "ChatComposer.h"
#include "LocalBridge.h"
#include "SpeechBubble.h"
#include "SshTunnel.h"
#include "TonyBehaviorEngine.h"

class QEnterEvent;

class PetWindow : public QWidget {
    Q_OBJECT
public:
    explicit PetWindow(QWidget *parent=nullptr);
    ~PetWindow() override;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    enum class Action {
        Idle, Curious, Pet, Carried, Land, Dizzy, Stretch, Yawn,
        Bob, Walk, Think, Celebrate, Sleep,
        Shiver, AskHug, Hug, Blush, BlushWave, Study,
        AdjustGlasses, RemoveGlasses, Wave
    };

    void loadAssets();
    QString assetKeyForAction(Action action) const;
    int frameStrideForAction(Action action) const;
    const QPixmap *pixmapForAction(Action action) const;
    void setAction(Action action, int durationMs=0);
    Action actionFromWire(const QString &name) const;
    Action baseActionForAgentState() const;
    void restoreAgentAction();
    void tickAnimation();
    void tickLife();
    void scheduleBlink();
    void scheduleIdleMoment();
    void runIdleMoment();
    void markInteraction();
    void handleTap(const QPoint &localPos);
    bool isHeadHit(const QPoint &localPos) const;
    void showLifeStatus();
    void askTony();
    void submitTonyPrompt(const QString &text);
    void hugTony();
    void configureConnection();
    void useLocalSshConnection();
    void showBubble(const QString &text, int timeoutMs=5200);
    void restorePosition();
    void savePosition();
    QString actionName() const;

    QPixmap pet_;
    QHash<QString,QPixmap> stateAssets_;
    QHash<QString,QVector<QPixmap>> animationAssets_;
    QTimer animTimer_;
    QTimer blinkTimer_;
    QTimer idleTimer_;
    QTimer actionTimer_;
    QTimer lifeTimer_;
    QTimer hoverTimer_;
    SshTunnel tunnel_;
    AgentClient agent_;
    QSystemTrayIcon tray_;
    LocalBridge localBridge_;
    SpeechBubble bubble_;
    ChatComposer composer_;
    TonyBehaviorEngine behavior_;
    Action action_{Action::Idle};
    QPoint dragOffset_;
    QPoint basePos_;
    QPoint pressGlobal_;
    QPoint lastDragGlobal_;
    QElapsedTimer lifeClock_;
    QElapsedTimer activityClock_;
    QElapsedTimer pressClock_;
    QElapsedTimer rapidClickClock_;
    bool dragging_{false};
    bool mouseDown_{false};
    bool hovered_{false};
    bool idleBlinking_{false};
    int idleBlinkTick_{0};
    int frame_{0};
    int walkDirection_{1};
    int dragTravel_{0};
    int rapidClicks_{0};
    int lifeSaveTicks_{0};
    QString answer_;
    QString emotion_{"neutral"};
    QString agentState_{"idle"};
};
