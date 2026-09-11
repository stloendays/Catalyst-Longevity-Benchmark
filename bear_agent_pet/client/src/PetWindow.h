#pragma once
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QPoint>
#include <QSystemTrayIcon>
#include "AgentClient.h"

class PetWindow : public QWidget {
    Q_OBJECT
public:
    explicit PetWindow(QWidget *parent=nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
private:
    enum class Action { Idle, Bob, Walk, Think, Celebrate, Sleep };
    void loadAsset();
    void setAction(Action action, int durationMs=0);
    void tickAnimation();
    void askTony();
    void showBubble(const QString &text);
    void restorePosition();
    void savePosition();
    QString actionName() const;

    QPixmap pet_;
    QTimer animTimer_;
    QTimer idleTimer_;
    QTimer actionTimer_;
    AgentClient agent_;
    QSystemTrayIcon tray_;
    Action action_{Action::Idle};
    QPoint dragOffset_;
    QPoint basePos_;
    bool dragging_{false};
    int frame_{0};
    QString answer_;
};
