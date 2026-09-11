#pragma once
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QPoint>
#include <QSystemTrayIcon>
#include "AgentClient.h"
#include "SshTunnel.h"

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
private:
    enum class Action {
        Idle, Bob, Walk, Think, Celebrate, Sleep,
        Shiver, AskHug, Hug, Blush, Study,
        AdjustGlasses, RemoveGlasses, Wave
    };

    void loadAsset();
    void setAction(Action action, int durationMs=0);
    Action actionFromWire(const QString &name) const;
    Action baseActionForAgentState() const;
    void restoreAgentAction();
    void tickAnimation();
    void askTony();
    void hugTony();
    void showBubble(const QString &text);
    void restorePosition();
    void savePosition();
    QString actionName() const;

    QPixmap pet_;
    QTimer animTimer_;
    QTimer idleTimer_;
    QTimer actionTimer_;
    SshTunnel tunnel_;
    AgentClient agent_;
    QSystemTrayIcon tray_;
    Action action_{Action::Idle};
    QPoint dragOffset_;
    QPoint basePos_;
    bool dragging_{false};
    int frame_{0};
    int walkDirection_{1};
    QString answer_;
    QString emotion_{"neutral"};
    QString agentState_{"idle"};
};
