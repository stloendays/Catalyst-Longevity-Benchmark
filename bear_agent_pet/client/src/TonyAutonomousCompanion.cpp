#include "TonyAutonomousCompanion.h"

#include "AppLogger.h"
#include "PetWindow.h"
#include "TonyResponsePack.h"

#include <QDateTime>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QStringList>

namespace {
QString normalizedMode(const QString &raw) {
    const QString value=raw.trimmed().toLower();
    if(value==QStringLiteral("off") || value==QStringLiteral("quiet") ||
       value==QStringLiteral("normal") || value==QStringLiteral("lively")) return value;
    return QStringLiteral("normal");
}

QString languageCode() {
    const QString value=QSettings().value(QStringLiteral("ui/language"),QStringLiteral("en")).toString().toLower();
    return value.startsWith(QStringLiteral("zh")) ? QStringLiteral("zh") : QStringLiteral("en");
}

struct Decision {
    QString intent;
    QString action;
    QString emotion;
    int durationMs{1800};
    int bubbleMs{4600};
    QStringList fallbackEnglish;
    QStringList fallbackChinese;
};

Decision makeDecision(const TonyBehaviorEngine::Snapshot &life,
                      int hour,
                      const QString &today,
                      bool morningAlreadyUsed,
                      bool paulaAlreadyUsed) {
    auto *rng=QRandomGenerator::global();
    const bool night=hour>=23 || hour<7;

    if(night) {
        return {
            QStringLiteral("autonomous_night"),QStringLiteral("yawn"),QStringLiteral("sleepy"),2200,4300,
            {QStringLiteral("It is pretty late. Tony is going to be quieter now."),
             QStringLiteral("Late-night mode. I will keep the desktop calm.")},
            {QStringLiteral("已经挺晚了。Tony 会安静一点。"),
             QStringLiteral("进入夜间模式啦，我会尽量不打扰你。")}
        };
    }

    if(life.warmth<=34 && rng->bounded(100)<84) {
        return {
            QStringLiteral("autonomous_cold"),QStringLiteral("shiver"),QStringLiteral("cold"),2400,4700,
            {QStringLiteral("Brrr... the air feels cold around my paws."),
             QStringLiteral("I think the air-conditioning is winning again.")},
            {QStringLiteral("有点冷……爪子都凉了。"),
             QStringLiteral("空调好像又赢了，Tony 有点冷。")}
        };
    }

    if(life.loneliness>=60 && rng->bounded(100)<80) {
        return {
            QStringLiteral("autonomous_lonely"),QStringLiteral("ask_hug"),QStringLiteral("hopeful"),2800,5000,
            {QStringLiteral("When you are free, can I borrow one small hug?"),
             QStringLiteral("I am still here, in case you forgot one small Tony on the desktop.")},
            {QStringLiteral("你忙完的话，可以借 Tony 一个小小的抱抱吗？"),
             QStringLiteral("我还在这里哦，别忘了桌面上还有一只 Tony。")}
        };
    }

    if(life.energy<=31 && rng->bounded(100)<76) {
        return {
            QStringLiteral("autonomous_sleepy"),QStringLiteral("yawn"),QStringLiteral("sleepy"),2200,4400,
            {QStringLiteral("That was not a yawn. It was... a very long breath."),
             QStringLiteral("Tony may need a tiny rest soon.")},
            {QStringLiteral("刚才不是打哈欠，只是……呼吸得比较久。"),
             QStringLiteral("Tony 好像快需要休息一下了。")}
        };
    }

    if(life.curiosity>=74 && rng->bounded(100)<72) {
        return {
            QStringLiteral("autonomous_study"),QStringLiteral("study"),QStringLiteral("focused"),3200,4900,
            {QStringLiteral("I am going to review a little chemistry while you work."),
             QStringLiteral("Quiet chemistry study break. You do your thing; I will do mine.")},
            {QStringLiteral("你忙你的，Tony 去复习一会儿化学。"),
             QStringLiteral("安静学习一会儿化学。你做你的，我做我的。")}
        };
    }

    if(hour>=7 && hour<11 && !morningAlreadyUsed) {
        return {
            QStringLiteral("autonomous_morning"),QStringLiteral("stretch"),QStringLiteral("content"),2200,4500,
            {QStringLiteral("Morning. One stretch, then Tony is officially operational."),
             QStringLiteral("Good morning. Glasses, curls, chemistry. Reasonable plan.")},
            {QStringLiteral("早。先伸个懒腰，Tony 就正式开机。"),
             QStringLiteral("早上好。眼镜、卷毛、化学，今天就这么开始吧。")}
        };
    }

    if(!paulaAlreadyUsed && rng->bounded(1000)<12) {
        return {
            QStringLiteral("autonomous_paula"),QStringLiteral("blush_wave"),QStringLiteral("bashful"),2600,4500,
            {QStringLiteral("...I randomly thought about Paula. Never mind."),
             QStringLiteral("I wonder what Paula is doing today. That was just a thought.")},
            {QStringLiteral("……突然想到 Paula 了。没什么。"),
             QStringLiteral("不知道 Paula 今天在做什么。Tony 只是随便想了一下。")}
        };
    }

    switch(rng->bounded(5)) {
    case 0:
        return {
            QStringLiteral("autonomous_observe"),QStringLiteral("head_tilt"),QStringLiteral("curious"),1800,4500,
            {QStringLiteral("You look busy. I will keep watch from here."),
             QStringLiteral("Tony is conducting highly scientific desktop observation.")},
            {QStringLiteral("你好像在忙。Tony 就在这里看着。"),
             QStringLiteral("Tony 正在进行非常科学的桌面观察。")}
        };
    case 1:
        return {
            QStringLiteral("autonomous_content"),QStringLiteral("nod"),QStringLiteral("content"),1500,4300,
            {QStringLiteral("This is nice. Just sitting here is fine too."),
             QStringLiteral("No emergency. Tony is simply content.")},
            {QStringLiteral("这样待着也挺好的。"),
             QStringLiteral("没有紧急情况，Tony 只是觉得现在挺舒服。")}
        };
    case 2:
        return {
            QStringLiteral("autonomous_walk"),QStringLiteral("walk"),QStringLiteral("playful"),3000,4500,
            {QStringLiteral("I have been sitting too long. Tiny patrol."),
             QStringLiteral("Short desktop inspection. I will be right back.")},
            {QStringLiteral("坐太久了，Tony 走两步。"),
             QStringLiteral("进行一次短距离桌面巡逻，马上回来。")}
        };
    case 3:
        return {
            QStringLiteral("autonomous_sniff"),QStringLiteral("sniff"),QStringLiteral("curious"),1900,4300,
            {QStringLiteral("Hmm. Did someone nearby open a snack?"),
             QStringLiteral("Tony detected something suspiciously snack-like.")},
            {QStringLiteral("嗯？附近是不是有人打开了零食？"),
             QStringLiteral("Tony 好像检测到了可疑的零食信号。")}
        };
    default:
        return {
            QStringLiteral("autonomous_study"),QStringLiteral("adjust_glasses"),QStringLiteral("focused"),1800,4600,
            {QStringLiteral("I should probably learn one more chemistry thing today."),
             QStringLiteral("Glasses adjusted. Time for one small chemistry thought.")},
            {QStringLiteral("今天好像还可以再学一个化学知识点。"),
             QStringLiteral("眼镜扶好。Tony 再想一个小小的化学问题。")}
        };
    }
}
}

TonyAutonomousCompanion::TonyAutonomousCompanion(PetWindow *pet, QObject *parent)
    : QObject(parent), pet_(pet), startedAtMs_(QDateTime::currentMSecsSinceEpoch()) {
    timer_.setSingleShot(true);
    connect(&timer_,&QTimer::timeout,this,[this]{ tick(); });
    scheduleNext();
}

QString TonyAutonomousCompanion::mode() const {
    return normalizedMode(QSettings().value(QStringLiteral("pet/autonomy_mode"),QStringLiteral("normal")).toString());
}

void TonyAutonomousCompanion::setMode(const QString &value) {
    const QString next=normalizedMode(value);
    QSettings().setValue(QStringLiteral("pet/autonomy_mode"),next);
    scheduleNext();
    AppLogger::recordOperatorEvent(QStringLiteral("autonomy_mode"),next);
}

void TonyAutonomousCompanion::announceMode() {
    if(!pet_) return;
    const QString current=mode();
    const bool zh=languageCode()==QStringLiteral("zh");
    QString text;
    if(current==QStringLiteral("off"))
        text=zh ? QStringLiteral("主动说话已关闭。我还是会安静地待在这里。") : QStringLiteral("Spontaneous speech is off. I will stay quietly on the desktop.");
    else if(current==QStringLiteral("quiet"))
        text=zh ? QStringLiteral("主动模式：安静。Tony 会很克制地偶尔说一句。") : QStringLiteral("Autonomy: Quiet. I will only speak occasionally.");
    else if(current==QStringLiteral("lively"))
        text=zh ? QStringLiteral("主动模式：活泼。Tony 会更常自己活动和说话。") : QStringLiteral("Autonomy: Lively. I will be a little more talkative.");
    else
        text=zh ? QStringLiteral("主动模式：正常。Tony 会偶尔根据状态自己说句话。") : QStringLiteral("Autonomy: Normal. I will occasionally speak when the moment fits.");
    pet_->showAutonomyNotice(text);
}

void TonyAutonomousCompanion::scheduleNext() {
    timer_.stop();
    const QString current=mode();
    if(current==QStringLiteral("off")) {
        timer_.start(15*60*1000);
        return;
    }

    auto *rng=QRandomGenerator::global();
    if(current==QStringLiteral("quiet")) timer_.start(rng->bounded(6*60*1000,11*60*1000+1));
    else if(current==QStringLiteral("lively")) timer_.start(rng->bounded(2*60*1000,270001));
    else timer_.start(rng->bounded(210000,7*60*1000+1));
}

void TonyAutonomousCompanion::tick() {
    scheduleNext();
    if(!pet_) return;

    const QString currentMode=mode();
    if(currentMode==QStringLiteral("off") || !pet_->autonomousSurfaceAvailable()) return;

    const qint64 nowMs=QDateTime::currentMSecsSinceEpoch();
    if(nowMs-startedAtMs_<3*60*1000) return;

    const qint64 inactiveMs=pet_->autonomyInactivityMs();
    const qint64 requiredIdle=currentMode==QStringLiteral("quiet") ? 6*60*1000
                              : currentMode==QStringLiteral("lively") ? 90*1000
                              : 3*60*1000;
    if(inactiveMs<requiredIdle) return;

    const QDateTime now=QDateTime::currentDateTime();
    const int hour=now.time().hour();
    const bool night=hour>=23 || hour<7;
    const QString bucket=now.toString(QStringLiteral("yyyyMMddHH"));
    if(hourBucket_!=bucket) {
        hourBucket_=bucket;
        spokenThisHour_=0;
    }

    int hourlyCap=currentMode==QStringLiteral("quiet") ? 2 : currentMode==QStringLiteral("lively") ? 6 : 4;
    if(night) hourlyCap=1;
    if(spokenThisHour_>=hourlyCap) return;

    const qint64 cooldownMs=currentMode==QStringLiteral("quiet") ? 8*60*1000
                             : currentMode==QStringLiteral("lively") ? 3*60*1000
                             : 5*60*1000;
    if(lastSpeechAtMs_>0 && nowMs-lastSpeechAtMs_<cooldownMs) return;

    auto *rng=QRandomGenerator::global();
    int chance=currentMode==QStringLiteral("quiet") ? 42 : currentMode==QStringLiteral("lively") ? 78 : 62;
    if(night) chance=currentMode==QStringLiteral("quiet") ? 4 : currentMode==QStringLiteral("lively") ? 22 : 12;
    if(rng->bounded(100)>=chance) return;

    const auto life=pet_->autonomySnapshot();
    const QString today=now.date().toString(QStringLiteral("yyyyMMdd"));
    const bool morningUsed=morningDate_==today;
    const bool paulaUsed=paulaDate_==today;
    const Decision decision=makeDecision(life,hour,today,morningUsed,paulaUsed);
    const QString line=TonyResponsePack::pick(decision.intent,languageCode(),decision.fallbackEnglish,decision.fallbackChinese);
    if(line.trimmed().isEmpty()) return;

    pet_->performAutonomousMoment(decision.action,decision.emotion,decision.durationMs,line,decision.bubbleMs);
    lastSpeechAtMs_=nowMs;
    ++spokenThisHour_;
    if(decision.intent==QStringLiteral("autonomous_morning")) morningDate_=today;
    if(decision.intent==QStringLiteral("autonomous_paula")) paulaDate_=today;

    AppLogger::recordOperatorEvent(
        QStringLiteral("autonomous_speech"),
        decision.intent,
        QJsonObject{
            {QStringLiteral("mode"),currentMode},
            {QStringLiteral("mood"),life.mood},
            {QStringLiteral("energy"),life.energy},
            {QStringLiteral("warmth"),life.warmth},
            {QStringLiteral("loneliness"),life.loneliness},
            {QStringLiteral("curiosity"),life.curiosity}
        });
}
