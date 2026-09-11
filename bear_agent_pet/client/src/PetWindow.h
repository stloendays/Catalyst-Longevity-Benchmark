#pragma once
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QPoint>
#include <QSystemTrayIcon>
#include <QHash>
#include <QVector>
#include "AgentClient.h"
#include "SshTunnel.h"
#include "SpeechBubble.h"
#include "ChatComposer.h"
#include "LocalBridge.h"

class QEnterEvent;

class PetWindow : public QWidget {
    Q_OBJECT
public:
    explicit PetWindow(QWidget *parent=nullptr);
    ~PetWindow() override;

public slots:
    // First-run setup calls this automatically. The public server address is built in,
    // so a trusted friend only has to enter the reusable friend code once.
    void configureConnection();
    void applyUiLanguage(const QString &language);
    void syncOperatorLogsForUpdate(const QString &targetVersion);

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
        Idle, Bob, Walk, Think, Celebrate, Sleep,
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
    void scheduleBlink();
    void scheduleIdleMoment();
    void runIdleMoment();
    void askTony();
    void submitTonyPrompt(const QString &text);
    void hugTony();
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
    SshTunnel tunnel_;
    AgentClient agent_;
    QSystemTrayIcon tray_;
    LocalBridge localBridge_;
    SpeechBubble bubble_;
    ChatComposer composer_;
    Action action_{Action::Idle};
    QPoint dragOffset_;
    QPoint basePos_;
    bool dragging_{false};
    bool idleBlinking_{false};
    int idleBlinkTick_{0};
    int frame_{0};
    int walkDirection_{1};
    QString answer_;
    QString emotion_{"neutral"};
    QString agentState_{"idle"};
};
