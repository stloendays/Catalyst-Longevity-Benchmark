#pragma once

#include <QJsonArray>
#include <QPoint>
#include <QWidget>

class QComboBox;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

class ChatComposer : public QWidget {
    Q_OBJECT
public:
    explicit ChatComposer(QWidget *parent=nullptr);

    void openAt(const QPoint &anchorGlobal, const QString &prefill={});
    void follow(const QPoint &anchorGlobal);
    void dismiss();
    void setLanguage(const QString &language);
    void setConversation(const QJsonArray &entries, const QString &streamingAssistant={});
    void setBusy(bool busy);
    void setHistoryCollapsed(bool collapsed);
    bool historyCollapsed() const;

signals:
    void submitted(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void submitCurrent();
    void placeNear(const QPoint &anchorGlobal);
    void refreshModelLabels();
    void refreshCollapseLabel();
    void rebuildTranscript();

    QLabel *title_{nullptr};
    QLabel *hint_{nullptr};
    QToolButton *collapse_{nullptr};
    QComboBox *modelBox_{nullptr};
    QPlainTextEdit *history_{nullptr};
    QLineEdit *edit_{nullptr};
    QPushButton *send_{nullptr};
    QPoint anchorGlobal_;
    QString language_{"en"};
    QJsonArray conversation_;
    QString streamingAssistant_;
    bool historyCollapsed_{false};
    bool busy_{false};
};
