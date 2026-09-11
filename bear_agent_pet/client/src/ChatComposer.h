#pragma once

#include <QPoint>
#include <QWidget>

class QKeyEvent;
class QLabel;
class QLineEdit;
class QPushButton;

class ChatComposer : public QWidget {
    Q_OBJECT
public:
    explicit ChatComposer(QWidget *parent=nullptr);

    void openAt(const QPoint &anchorGlobal, const QString &prefill={});
    void follow(const QPoint &anchorGlobal);
    void dismiss();

signals:
    void submitted(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void submitCurrent();
    void placeNear(const QPoint &anchorGlobal);

    QLabel *title_{nullptr};
    QLabel *hint_{nullptr};
    QLineEdit *edit_{nullptr};
    QPushButton *send_{nullptr};
    QPoint anchorGlobal_;
};
