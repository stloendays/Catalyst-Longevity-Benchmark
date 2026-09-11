#include "ChatComposer.h"

#include "AppLogger.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

ChatComposer::ChatComposer(QWidget *parent): QWidget(parent) {
    setWindowTitle("Tony");
    setObjectName("tonyComposer");
    setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground,true);
    setFixedWidth(360);

    title_=new QLabel("Tony",this);
    title_->setObjectName("title");
    hint_=new QLabel("What do you want to tell me?",this);
    hint_->setObjectName("hint");
    edit_=new QLineEdit(this);
    edit_->setObjectName("message");
    edit_->setPlaceholderText("Type a message…");
    edit_->setClearButtonEnabled(true);
    send_=new QPushButton("Send",this);
    send_->setObjectName("send");
    send_->setCursor(Qt::PointingHandCursor);

    auto *top=new QHBoxLayout;
    top->setContentsMargins(0,0,0,0);
    top->addWidget(title_);
    top->addStretch(1);
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
    AppLogger::recordOperatorEvent(
        QStringLiteral("chat_submit"),
        text,
        QJsonObject{{QStringLiteral("language"), language_}});
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

    int x=anchorGlobal.x()-width()/2;
    int y=anchorGlobal.y()-height()-12;
    if(y<area.top()+8) y=anchorGlobal.y()+26;
    x=qBound(area.left()+8,x,area.right()-width()-8);
    y=qBound(area.top()+8,y,area.bottom()-height()-8);
    move(x,y);
}
