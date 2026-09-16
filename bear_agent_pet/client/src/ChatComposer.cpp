#include "ChatComposer.h"

#include "AppLogger.h"

#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QVBoxLayout>

namespace {
QString normalizedModelProfile(const QString &value) {
    const QString profile=value.trimmed().toLower();
    if(profile==QStringLiteral("fast") || profile==QStringLiteral("quality")) return profile;
    return QStringLiteral("auto");
}
}

ChatComposer::ChatComposer(QWidget *parent): QWidget(parent) {
    setWindowTitle("Tony");
    setObjectName("tonyComposer");
    setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground,true);
    setFixedWidth(440);

    title_=new QLabel("Tony",this);
    title_->setObjectName("title");
    hint_=new QLabel("What do you want to tell me?",this);
    hint_->setObjectName("hint");

    modelBox_=new QComboBox(this);
    modelBox_->setObjectName("modelProfile");
    modelBox_->setCursor(Qt::PointingHandCursor);
    modelBox_->setToolTip("Choose which local model Tony should use for this conversation.");
    modelBox_->addItem(QStringLiteral("Auto"),QStringLiteral("auto"));
    modelBox_->addItem(QStringLiteral("Fast 0.8B"),QStringLiteral("fast"));
    modelBox_->addItem(QStringLiteral("Quality 2B"),QStringLiteral("quality"));
    const QString savedProfile=normalizedModelProfile(
        QSettings().value(QStringLiteral("agent/model_profile"),QStringLiteral("auto")).toString());
    const int savedIndex=modelBox_->findData(savedProfile);
    modelBox_->setCurrentIndex(savedIndex>=0 ? savedIndex : 0);

    edit_=new QLineEdit(this);
    edit_->setObjectName("message");
    edit_->setPlaceholderText("Type a message…");
    edit_->setClearButtonEnabled(true);
    send_=new QPushButton("Send",this);
    send_->setObjectName("send");
    send_->setCursor(Qt::PointingHandCursor);

    auto *top=new QHBoxLayout;
    top->setContentsMargins(0,0,0,0);
    top->setSpacing(8);
    top->addWidget(title_);
    top->addStretch(1);
    top->addWidget(modelBox_);
    top->addWidget(hint_);

    auto *row=new QHBoxLayout;
    row->setContentsMargins(0,0,0,0);
    row->setSpacing(8);
    row->addWidget(edit_,1);
    row->addWidget(send_);

    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(16,13,16,14);
    layout->setSpacing(10);
    layout->addLayout(top);
    layout->addLayout(row);

    setStyleSheet(R"CSS(
        QWidget#tonyComposer {
            background: rgba(250, 247, 243, 248);
            border: 1px solid rgba(112, 91, 74, 120);
            border-radius: 18px;
        }
        QLabel#title {
            color: #332a24;
            font-family: "Microsoft YaHei UI";
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#hint {
            color: rgba(70, 61, 55, 170);
            font-family: "Microsoft YaHei UI";
            font-size: 11px;
        }
        QComboBox#modelProfile {
            min-height: 28px;
            padding: 0 8px;
            color: #493d34;
            background: rgba(255, 255, 255, 220);
            border: 1px solid rgba(119, 102, 88, 72);
            border-radius: 9px;
            font-family: "Microsoft YaHei UI";
            font-size: 10px;
        }
        QComboBox#modelProfile:hover {
            border: 1px solid rgba(126, 91, 65, 150);
        }
        QLineEdit#message {
            min-height: 36px;
            padding: 0 12px;
            color: #2c2825;
            background: rgba(255, 255, 255, 235);
            border: 1px solid rgba(119, 102, 88, 80);
            border-radius: 12px;
            selection-background-color: #c7aa92;
            font-family: "Microsoft YaHei UI";
            font-size: 12px;
        }
        QLineEdit#message:focus {
            border: 1px solid rgba(126, 91, 65, 185);
        }
        QPushButton#send {
            min-width: 66px;
            min-height: 36px;
            padding: 0 10px;
            color: white;
            background: #6f5949;
            border: none;
            border-radius: 12px;
            font-family: "Microsoft YaHei UI";
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#send:hover { background: #5f493c; }
        QPushButton#send:pressed { background: #503d33; }
    )CSS");

    connect(send_,&QPushButton::clicked,this,&ChatComposer::submitCurrent);
    connect(edit_,&QLineEdit::returnPressed,this,&ChatComposer::submitCurrent);
    connect(modelBox_,&QComboBox::currentIndexChanged,this,[this](int index){
        const QString profile=normalizedModelProfile(modelBox_->itemData(index).toString());
        QSettings().setValue(QStringLiteral("agent/model_profile"),profile);
        AppLogger::recordOperatorEvent(
            QStringLiteral("model_profile_changed"),{},
            QJsonObject{{QStringLiteral("profile"),profile}});
    });
    refreshModelLabels();
}

void ChatComposer::refreshModelLabels() {
    if(!modelBox_) return;
    const QString current=normalizedModelProfile(modelBox_->currentData().toString());
    modelBox_->setItemText(0,language_=="zh" ? QStringLiteral("自动") : QStringLiteral("Auto"));
    modelBox_->setItemText(1,language_=="zh" ? QStringLiteral("快速 0.8B") : QStringLiteral("Fast 0.8B"));
    modelBox_->setItemText(2,language_=="zh" ? QStringLiteral("质量 2B") : QStringLiteral("Quality 2B"));
    const int index=modelBox_->findData(current);
    if(index>=0) modelBox_->setCurrentIndex(index);
    modelBox_->setToolTip(language_=="zh"
        ? QStringLiteral("自动：普通聊天用 0.8B，技术问题用 2B；也可以强制指定模型。")
        : QStringLiteral("Auto uses 0.8B for casual chat and 2B for technical questions; you can also force either model."));
}

void ChatComposer::setLanguage(const QString &language) {
    const QString n=language.trimmed().toLower();
    language_=(n.startsWith("zh") || n=="cn") ? "zh" : "en";
    if(language_=="zh") {
        hint_->setText("想对我说什么？");
        edit_->setPlaceholderText("输入消息…");
        send_->setText("发送");
    } else {
        hint_->setText("What do you want to tell me?");
        edit_->setPlaceholderText("Type a message…");
        send_->setText("Send");
    }
    refreshModelLabels();
}

void ChatComposer::openAt(const QPoint &anchorGlobal, const QString &prefill) {
    anchorGlobal_=anchorGlobal;
    if(!prefill.isNull()) edit_->setText(prefill);
    adjustSize();
    placeNear(anchorGlobal_);
    show();
    raise();
    activateWindow();
    edit_->setFocus(Qt::OtherFocusReason);
    edit_->selectAll();
}

void ChatComposer::follow(const QPoint &anchorGlobal) {
    anchorGlobal_=anchorGlobal;
    if(isVisible()) placeNear(anchorGlobal_);
}

void ChatComposer::dismiss() {
    hide();
}

void ChatComposer::keyPressEvent(QKeyEvent *event) {
    if(event->key()==Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ChatComposer::submitCurrent() {
    const QString text=edit_->text().trimmed();
    if(text.isEmpty()) return;
    const QString profile=normalizedModelProfile(modelBox_->currentData().toString());
    AppLogger::recordOperatorEvent(
        QStringLiteral("chat_submit"),
        text,
        QJsonObject{{QStringLiteral("language"), language_},
                    {QStringLiteral("model_profile"),profile}});
    edit_->clear();
    hide();
    emit submitted(text);
}

void ChatComposer::placeNear(const QPoint &anchorGlobal) {
    auto *screen=QGuiApplication::screenAt(anchorGlobal);
    if(!screen) screen=QGuiApplication::primaryScreen();
    const QRect area=screen ? screen->availableGeometry() : QRect(anchorGlobal-QPoint(600,400),QSize(1200,800));

    const int h=sizeHint().height()>0 ? sizeHint().height() : 92;
    resize(width(),h);

    const int edge=8;
    const int petHalfWidthPlusGap=156;
    int x=anchorGlobal.x()-width()/2;
    int y=anchorGlobal.y()-height()-14;

    // Never fall back directly on top of Tony. Near the top of the screen the
    // composer moves to whichever side has room.
    if(y<area.top()+edge) {
        const int leftX=anchorGlobal.x()-petHalfWidthPlusGap-width();
        const int rightX=anchorGlobal.x()+petHalfWidthPlusGap;
        if(leftX>=area.left()+edge) x=leftX;
        else if(rightX+width()<=area.right()-edge) x=rightX;
        else x=qBound(area.left()+edge,x,area.right()-width()-edge);
        y=qBound(area.top()+edge,anchorGlobal.y()+8,area.bottom()-height()-edge);
    }

    x=qBound(area.left()+edge,x,area.right()-width()-edge);
    y=qBound(area.top()+edge,y,area.bottom()-height()-edge);
    move(x,y);
}
