#include "TodayDialog.h"

#include "NewsCompanion.h"
#include "PetWindow.h"

#include <QDate>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

TodayDialog::TodayDialog(PetWindow *pet, NewsCompanion *news, QWidget *parent)
    : QDialog(parent), pet_(pet), news_(news) {
    setModal(false);
    resize(620, 690);
    setMinimumSize(540, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    dateLabel_ = new QLabel(this);
    dateLabel_->setObjectName(QStringLiteral("todayDate"));
    summaryLabel_ = new QLabel(this);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setObjectName(QStringLiteral("todaySummary"));

    root->addWidget(dateLabel_);
    root->addWidget(summaryLabel_);

    auto *quickRow = new QHBoxLayout;
    quickRow->setSpacing(8);
    chat_ = new QPushButton(this);
    morningBrief_ = new QPushButton(this);
    eveningBrief_ = new QPushButton(this);
    quickRow->addWidget(chat_);
    quickRow->addWidget(morningBrief_);
    quickRow->addWidget(eveningBrief_);
    root->addLayout(quickRow);

    remindersTitle_ = new QLabel(this);
    remindersTitle_->setObjectName(QStringLiteral("sectionTitle"));
    reminders_ = new QListWidget(this);
    reminders_->setMinimumHeight(120);
    reminders_->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *reminderButtons = new QHBoxLayout;
    addReminder_ = new QPushButton(this);
    cancelReminder_ = new QPushButton(this);
    reminderButtons->addWidget(addReminder_);
    reminderButtons->addWidget(cancelReminder_);
    reminderButtons->addStretch(1);

    root->addWidget(remindersTitle_);
    root->addWidget(reminders_);
    root->addLayout(reminderButtons);

    newsLabel_ = new QLabel(this);
    newsLabel_->setWordWrap(true);
    newsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *newsButtons = new QHBoxLayout;
    checkNews_ = new QPushButton(this);
    openNews_ = new QPushButton(this);
    newsButtons->addWidget(checkNews_);
    newsButtons->addWidget(openNews_);
    newsButtons->addStretch(1);

    root->addWidget(newsLabel_);
    root->addLayout(newsButtons);

    conversationTitle_ = new QLabel(this);
    conversationTitle_->setObjectName(QStringLiteral("sectionTitle"));
    conversation_ = new QPlainTextEdit(this);
    conversation_->setReadOnly(true);
    conversation_->setMinimumHeight(150);
    conversation_->setMaximumBlockCount(120);

    auto *conversationButtons = new QHBoxLayout;
    clearHistory_ = new QPushButton(this);
    conversationButtons->addWidget(clearHistory_);
    conversationButtons->addStretch(1);

    root->addWidget(conversationTitle_);
    root->addWidget(conversation_, 1);
    root->addLayout(conversationButtons);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    close_ = buttons->button(QDialogButtonBox::Close);
    root->addWidget(buttons);

    setStyleSheet(QStringLiteral(R"CSS(
        QDialog {
            background: #f7f4f0;
            color: #332a24;
            font-family: "Microsoft YaHei UI";
        }
        QLabel#todayDate {
            font-size: 21px;
            font-weight: 700;
        }
        QLabel#todaySummary {
            color: #66584d;
            font-size: 12px;
        }
        QLabel#sectionTitle {
            font-size: 13px;
            font-weight: 700;
            margin-top: 4px;
        }
        QListWidget, QPlainTextEdit {
            background: rgba(255,255,255,235);
            border: 1px solid rgba(119,102,88,70);
            border-radius: 12px;
            padding: 7px;
        }
        QPushButton {
            min-height: 32px;
            padding: 0 11px;
            border: 1px solid rgba(119,102,88,70);
            border-radius: 10px;
            background: rgba(255,255,255,235);
            color: #493d34;
        }
        QPushButton:hover {
            border-color: rgba(126,91,65,150);
            background: #ffffff;
        }
    )CSS"));

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    connect(chat_, &QPushButton::clicked, this, [this]{
        if(pet_) pet_->openChat();
    });
    connect(morningBrief_, &QPushButton::clicked, this, [this]{
        if(pet_) pet_->requestDailyBrief(
            QStringLiteral("morning"),
            news_ ? news_->latestStoryTitle() : QString(),
            news_ ? news_->latestStorySource() : QString());
    });
    connect(eveningBrief_, &QPushButton::clicked, this, [this]{
        if(pet_) pet_->requestDailyBrief(
            QStringLiteral("evening"),
            news_ ? news_->latestStoryTitle() : QString(),
            news_ ? news_->latestStorySource() : QString());
    });
    connect(addReminder_, &QPushButton::clicked, this, [this]{
        if(!pet_) return;
        pet_->createQuickReminder();
        refresh();
    });
    connect(cancelReminder_, &QPushButton::clicked, this, [this]{
        if(!pet_) return;
        auto *item = reminders_->currentItem();
        if(!item) return;
        const QString id = item->data(Qt::UserRole).toString();
        if(id.isEmpty()) return;

        const bool zh = chinese();
        if(QMessageBox::question(
               this,
               zh ? QStringLiteral("取消提醒") : QStringLiteral("Cancel reminder"),
               zh ? QStringLiteral("确定取消选中的提醒吗？")
                  : QStringLiteral("Cancel the selected reminder?"),
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) != QMessageBox::Yes)
            return;

        pet_->cancelLocalReminder(id);
        refreshReminders();
    });
    connect(checkNews_, &QPushButton::clicked, this, [this]{
        if(news_) news_->fetchNow(true);
    });
    connect(openNews_, &QPushButton::clicked, this, [this]{
        if(news_) news_->openLatestStory();
    });
    connect(clearHistory_, &QPushButton::clicked, this, [this]{
        if(!pet_) return;
        const bool zh = chinese();
        if(QMessageBox::question(
               this,
               zh ? QStringLiteral("清空对话历史") : QStringLiteral("Clear conversation history"),
               zh ? QStringLiteral("只会清空 Today 面板中的本地聊天历史，不会删除 Tony 的显式记忆。继续吗？")
                  : QStringLiteral("This only clears local chat history shown in Today. Tony's explicit memories are not deleted. Continue?"),
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) != QMessageBox::Yes)
            return;

        pet_->clearConversationHistory();
        refreshConversation();
    });

    if(news_) {
        connect(news_, &NewsCompanion::latestStoryChanged, this,
                [this](const QString &, const QString &, const QUrl &){
            refreshNews();
        });
    }

    auto *refreshTimer = new QTimer(this);
    refreshTimer->setInterval(30000);
    connect(refreshTimer, &QTimer::timeout, this, [this]{
        if(isVisible()) refresh();
    });
    refreshTimer->start();

    refresh();
}

bool TodayDialog::chinese() const {
    return QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en"))
        .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
}

void TodayDialog::refresh() {
    rebuildText();
    refreshReminders();
    refreshConversation();
    refreshNews();
}

void TodayDialog::rebuildText() {
    const bool zh = chinese();
    setWindowTitle(zh ? QStringLiteral("Tony · 今日")
                      : QStringLiteral("Tony · Today"));

    const QDate today = QDate::currentDate();
    dateLabel_->setText(
        zh ? QStringLiteral("今天 · %1").arg(today.toString(QStringLiteral("yyyy 年 M 月 d 日")))
           : QStringLiteral("Today · %1").arg(today.toString(QStringLiteral("ddd, d MMM yyyy"))));

    const int reminderCount = pet_ ? pet_->localReminders().size() : 0;
    const int historyCount = pet_ ? pet_->recentConversation().size() : 0;
    summaryLabel_->setText(
        zh ? QStringLiteral("Tony 把今天常用的信息收在这里：%1 个待提醒事项，%2 条本地对话记录。")
                 .arg(reminderCount).arg(historyCount)
           : QStringLiteral("Tony keeps today's essentials here: %1 upcoming reminder(s), %2 local conversation entries.")
                 .arg(reminderCount).arg(historyCount));

    chat_->setText(zh ? QStringLiteral("和 Tony 聊天") : QStringLiteral("Chat"));
    morningBrief_->setText(zh ? QStringLiteral("早间简报") : QStringLiteral("Morning Brief"));
    eveningBrief_->setText(zh ? QStringLiteral("晚间简报") : QStringLiteral("Evening Brief"));
    remindersTitle_->setText(zh ? QStringLiteral("接下来的提醒") : QStringLiteral("Upcoming reminders"));
    addReminder_->setText(zh ? QStringLiteral("新建提醒…") : QStringLiteral("New reminder…"));
    cancelReminder_->setText(zh ? QStringLiteral("取消选中") : QStringLiteral("Cancel selected"));
    checkNews_->setText(zh ? QStringLiteral("现在查新闻") : QStringLiteral("Check news now"));
    openNews_->setText(zh ? QStringLiteral("打开最近原文") : QStringLiteral("Open latest story"));
    conversationTitle_->setText(zh ? QStringLiteral("最近对话") : QStringLiteral("Recent conversation"));
    clearHistory_->setText(zh ? QStringLiteral("清空本地对话历史") : QStringLiteral("Clear local history"));
    close_->setText(zh ? QStringLiteral("关闭") : QStringLiteral("Close"));
}

void TodayDialog::refreshReminders() {
    reminders_->clear();
    if(!pet_) return;

    const bool zh = chinese();
    const QJsonArray rows = pet_->localReminders();
    for(const auto &value : rows) {
        const QJsonObject row = value.toObject();
        const QString id = row.value(QStringLiteral("id")).toString();
        const QString title = row.value(QStringLiteral("title")).toString();
        const QString text = row.value(QStringLiteral("text")).toString();
        const QDateTime due = QDateTime::fromString(
            row.value(QStringLiteral("due_utc")).toString(), Qt::ISODate).toLocalTime();

        const QString when = due.isValid()
            ? due.toString(QStringLiteral("MM-dd HH:mm"))
            : (zh ? QStringLiteral("时间未知") : QStringLiteral("unknown time"));
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  ·  %2%3")
                .arg(when,
                     text.isEmpty() ? title : text,
                     (!title.isEmpty() && !text.isEmpty() && title != text)
                         ? QStringLiteral("  [%1]").arg(title)
                         : QString()),
            reminders_);
        item->setData(Qt::UserRole, id);
    }

    if(rows.isEmpty()) {
        auto *item = new QListWidgetItem(
            zh ? QStringLiteral("暂时没有待提醒事项。")
               : QStringLiteral("No upcoming reminders."),
            reminders_);
        item->setFlags(Qt::NoItemFlags);
    }
    cancelReminder_->setEnabled(!rows.isEmpty());
}

void TodayDialog::refreshConversation() {
    conversation_->clear();
    if(!pet_) return;

    const bool zh = chinese();
    const QJsonArray rows = pet_->recentConversation();
    const int start = qMax(0, rows.size() - 14);
    QStringList lines;
    for(int i = start; i < rows.size(); ++i) {
        const QJsonObject row = rows.at(i).toObject();
        const bool assistant = row.value(QStringLiteral("role")).toString() == QStringLiteral("assistant");
        const QString who = assistant
            ? QStringLiteral("Tony")
            : (zh ? QStringLiteral("你") : QStringLiteral("You"));
        const QString text = row.value(QStringLiteral("text")).toString().simplified();
        if(!text.isEmpty())
            lines << QStringLiteral("%1: %2").arg(who, text);
    }

    conversation_->setPlainText(
        lines.isEmpty()
            ? (zh ? QStringLiteral("还没有可回看的对话。")
                  : QStringLiteral("No conversation history yet."))
            : lines.join(QStringLiteral("\n\n")));
    clearHistory_->setEnabled(!rows.isEmpty());
}

void TodayDialog::refreshNews() {
    const bool zh = chinese();
    if(!news_ || news_->latestStoryTitle().trimmed().isEmpty()) {
        newsLabel_->setText(
            zh ? QStringLiteral("最近新闻：暂无。可以让 Tony 现在查一条。")
               : QStringLiteral("Latest news: none yet. Tony can check now."));
        openNews_->setEnabled(false);
        return;
    }

    newsLabel_->setText(
        zh ? QStringLiteral("最近新闻 · %1\n%2")
                 .arg(news_->latestStorySource(), news_->latestStoryTitle())
           : QStringLiteral("Latest news · %1\n%2")
                 .arg(news_->latestStorySource(), news_->latestStoryTitle()));
    openNews_->setEnabled(news_->latestStoryUrl().isValid());
}
