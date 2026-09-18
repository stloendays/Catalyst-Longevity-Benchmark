#include "NewsSettingsDialog.h"

#include "NewsCompanion.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

NewsSettingsDialog::NewsSettingsDialog(NewsCompanion *news, QWidget *parent)
    : QDialog(parent), news_(news) {
    setModal(false);
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);

    intro_ = new QLabel(this);
    intro_->setWordWrap(true);
    root->addWidget(intro_);

    enabled_ = new QCheckBox(this);
    root->addWidget(enabled_);

    auto *scheduleGroup = new QGroupBox(this);
    auto *scheduleForm = new QFormLayout(scheduleGroup);
    scheduleForm->setSpacing(10);

    frequency_ = new QComboBox(scheduleGroup);
    frequency_->addItem(QString(), 60);
    frequency_->addItem(QString(), 120);
    frequency_->addItem(QString(), 240);
    scheduleForm->addRow(QString(), frequency_);

    quietStart_ = new QSpinBox(scheduleGroup);
    quietEnd_ = new QSpinBox(scheduleGroup);
    for(auto *box : {quietStart_, quietEnd_}) {
        box->setRange(0, 23);
        box->setSuffix(QStringLiteral(":00"));
    }
    scheduleForm->addRow(QString(), quietStart_);
    scheduleForm->addRow(QString(), quietEnd_);

    quietHint_ = new QLabel(scheduleGroup);
    quietHint_->setWordWrap(true);
    scheduleForm->addRow(QString(), quietHint_);
    root->addWidget(scheduleGroup);

    auto *sourcesGroup = new QGroupBox(this);
    auto *sourcesLayout = new QVBoxLayout(sourcesGroup);
    if(news_) {
        for(const auto &source : news_->sources()) {
            auto *box = new QCheckBox(source.name, sourcesGroup);
            sourceBoxes_.insert(source.id, box);
            sourcesLayout->addWidget(box);
            connect(box, &QCheckBox::toggled, this, [this,id=source.id](bool checked){
                if(news_) news_->setSourceEnabled(id, checked);
            });
        }
    }
    root->addWidget(sourcesGroup);

    latest_ = new QLabel(this);
    latest_->setWordWrap(true);
    latest_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(latest_);

    auto *actions = new QHBoxLayout;
    checkNow_ = new QPushButton(this);
    openLatest_ = new QPushButton(this);
    actions->addWidget(checkNow_);
    actions->addWidget(openLatest_);
    actions->addStretch(1);
    root->addLayout(actions);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    close_ = buttons->button(QDialogButtonBox::Close);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    connect(checkNow_, &QPushButton::clicked, this, [this]{
        if(news_) news_->fetchNow(true);
    });
    connect(openLatest_, &QPushButton::clicked, this, [this]{
        if(news_) news_->openLatestStory();
    });
    connect(enabled_, &QCheckBox::toggled, this, [this](bool checked){
        if(news_) news_->setEnabled(checked);
    });
    connect(frequency_, &QComboBox::currentIndexChanged, this, [this](int){
        if(news_) news_->setIntervalMinutes(frequency_->currentData().toInt());
    });
    connect(quietStart_, &QSpinBox::valueChanged, this, [this](int){
        if(news_) news_->setQuietHours(quietStart_->value(), quietEnd_->value());
    });
    connect(quietEnd_, &QSpinBox::valueChanged, this, [this](int){
        if(news_) news_->setQuietHours(quietStart_->value(), quietEnd_->value());
    });

    if(news_) {
        connect(news_, &NewsCompanion::enabledChanged, this, [this](bool){
            refresh();
        });
        connect(news_, &NewsCompanion::intervalChanged, this, [this](int){
            refresh();
        });
        connect(news_, &NewsCompanion::latestStoryChanged, this,
                [this](const QString &, const QString &, const QUrl &){
            refreshLatest();
        });
    }

    refresh();
}

bool NewsSettingsDialog::chinese() const {
    return QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en"))
        .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
}

void NewsSettingsDialog::refresh() {
    if(!news_) return;

    const QSignalBlocker blockEnabled(enabled_);
    const QSignalBlocker blockFrequency(frequency_);
    const QSignalBlocker blockStart(quietStart_);
    const QSignalBlocker blockEnd(quietEnd_);

    enabled_->setChecked(news_->enabled());

    const int interval = news_->intervalMinutes();
    for(int i = 0; i < frequency_->count(); ++i) {
        if(frequency_->itemData(i).toInt() == interval) {
            frequency_->setCurrentIndex(i);
            break;
        }
    }

    quietStart_->setValue(news_->quietStartHour());
    quietEnd_->setValue(news_->quietEndHour());

    for(auto it = sourceBoxes_.begin(); it != sourceBoxes_.end(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setChecked(news_->sourceEnabled(it.key()));
    }

    rebuildText();
    refreshLatest();
}

void NewsSettingsDialog::rebuildText() {
    const bool zh = chinese();
    setWindowTitle(zh ? QStringLiteral("Tony 联网新闻")
                      : QStringLiteral("Tony News Companion"));

    intro_->setText(
        zh ? QStringLiteral("Tony 只会偶尔分享没说过的新标题；打开原文仍由你决定。自动新闻默认可以随时关闭。")
           : QStringLiteral("Tony only shares occasional unseen headlines. Opening the original article is always your choice, and automatic news can be paused anytime."));

    enabled_->setText(zh ? QStringLiteral("允许 Tony 自动联网查看并分享新闻")
                         : QStringLiteral("Let Tony automatically check and share news"));

    auto *scheduleGroup = qobject_cast<QGroupBox*>(frequency_->parentWidget());
    if(scheduleGroup) scheduleGroup->setTitle(
        zh ? QStringLiteral("频率与安静时段") : QStringLiteral("Frequency & quiet hours"));

    auto *form = qobject_cast<QFormLayout*>(scheduleGroup ? scheduleGroup->layout() : nullptr);
    if(form) {
        if(auto *label = qobject_cast<QLabel*>(form->labelForField(frequency_)))
            label->setText(zh ? QStringLiteral("最多播报") : QStringLiteral("Maximum cadence"));
        if(auto *label = qobject_cast<QLabel*>(form->labelForField(quietStart_)))
            label->setText(zh ? QStringLiteral("安静开始") : QStringLiteral("Quiet starts"));
        if(auto *label = qobject_cast<QLabel*>(form->labelForField(quietEnd_)))
            label->setText(zh ? QStringLiteral("安静结束") : QStringLiteral("Quiet ends"));
    }

    frequency_->setItemText(0, zh ? QStringLiteral("每小时最多一条")
                                  : QStringLiteral("At most once per hour"));
    frequency_->setItemText(1, zh ? QStringLiteral("每两小时最多一条")
                                  : QStringLiteral("At most every 2 hours"));
    frequency_->setItemText(2, zh ? QStringLiteral("每四小时最多一条")
                                  : QStringLiteral("At most every 4 hours"));

    quietHint_->setText(
        zh ? QStringLiteral("在安静时段或“免打扰”开启时，Tony 不会自动播报；手动“现在查一条”仍然可用。")
           : QStringLiteral("Automatic briefings pause during quiet hours or Do Not Disturb. Manual checks still work."));

    if(auto *sourcesGroup = qobject_cast<QGroupBox*>(
           sourceBoxes_.isEmpty() ? nullptr : sourceBoxes_.constBegin().value()->parentWidget())) {
        sourcesGroup->setTitle(zh ? QStringLiteral("新闻源") : QStringLiteral("Sources"));
    }

    checkNow_->setText(zh ? QStringLiteral("现在查一条") : QStringLiteral("Check now"));
    openLatest_->setText(zh ? QStringLiteral("打开最近原文") : QStringLiteral("Open latest story"));
    close_->setText(zh ? QStringLiteral("关闭") : QStringLiteral("Close"));
}

void NewsSettingsDialog::refreshLatest() {
    if(!news_) return;
    const bool zh = chinese();
    const QString title = news_->latestStoryTitle().trimmed();
    const QString source = news_->latestStorySource().trimmed();

    if(title.isEmpty()) {
        latest_->setText(zh ? QStringLiteral("最近新闻：暂无")
                            : QStringLiteral("Latest story: none yet"));
        openLatest_->setEnabled(false);
        return;
    }

    latest_->setText(
        zh ? QStringLiteral("最近新闻 · %1\n%2").arg(source, title)
           : QStringLiteral("Latest · %1\n%2").arg(source, title));
    openLatest_->setEnabled(news_->latestStoryUrl().isValid());
}
