#pragma once

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;
class QPlainTextEdit;
class NewsCompanion;
class PetWindow;

class TodayDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TodayDialog(PetWindow *pet, NewsCompanion *news, QWidget *parent=nullptr);
    void refresh();

private:
    bool chinese() const;
    void rebuildText();
    void refreshReminders();
    void refreshConversation();
    void refreshNews();

    PetWindow *pet_{nullptr};
    NewsCompanion *news_{nullptr};

    QLabel *dateLabel_{nullptr};
    QLabel *summaryLabel_{nullptr};
    QLabel *newsLabel_{nullptr};
    QLabel *remindersTitle_{nullptr};
    QLabel *conversationTitle_{nullptr};

    QListWidget *reminders_{nullptr};
    QPlainTextEdit *conversation_{nullptr};

    QPushButton *chat_{nullptr};
    QPushButton *morningBrief_{nullptr};
    QPushButton *eveningBrief_{nullptr};
    QPushButton *addReminder_{nullptr};
    QPushButton *cancelReminder_{nullptr};
    QPushButton *checkNews_{nullptr};
    QPushButton *openNews_{nullptr};
    QPushButton *clearHistory_{nullptr};
    QPushButton *close_{nullptr};
};
