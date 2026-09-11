#pragma once

#include <QTimer>
#include <QWidget>

class SpeechBubble : public QWidget {
    Q_OBJECT
public:
    explicit SpeechBubble(QWidget *parent=nullptr);

    void showMessage(const QString &text, const QPoint &anchorGlobal, const QString &tone="neutral", int timeoutMs=5200);
    void follow(const QPoint &anchorGlobal);
    void dismiss();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateGeometryForText();
    void placeNear(const QPoint &anchorGlobal);

    QString text_;
    QString tone_{"neutral"};
    QPoint anchorGlobal_;
    QTimer hideTimer_;
};
