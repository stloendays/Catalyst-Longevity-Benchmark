#include "TonyResponseRouter.h"

#include <QRandomGenerator>
#include <QStringList>

namespace {
QString pick(const QStringList &items) {
    if(items.isEmpty()) return {};
    return items.at(QRandomGenerator::global()->bounded(items.size()));
}

bool hasAny(const QString &text, const QStringList &tokens) {
    for(const auto &token : tokens) {
        if(text.contains(token, Qt::CaseInsensitive)) return true;
    }
    return false;
}

QString localize(bool zh, const QStringList &en, const QStringList &cn) {
    return pick(zh ? cn : en);
}
}

TonyResponseRouter::Decision TonyResponseRouter::resolve(
    const QString &prompt,
    const QString &language,
    bool agentConnected,
    const TonyBehaviorEngine::Snapshot &state) const {

    Decision out;
    out.forwardText = prompt.trimmed();
    const QString p = out.forwardText.toLower();
    const bool zh = language.trimmed().toLower().startsWith("zh");

    auto fixed = [&](const QString &intent, const QStringList &en, const QStringList &cn,
                     const QString &action, const QString &emotion, int duration = 2200) {
        out.route = Route::LocalFixed;
        out.intent = intent;
        out.reply = localize(zh, en, cn);
        out.action = action;
        out.emotion = emotion;
        out.durationMs = duration;
        return out;
    };

    auto action = [&](const QString &intent, const QStringList &en, const QStringList &cn,
                      const QString &actionName, const QString &emotion, int duration = 2600) {
        out.route = Route::LocalAction;
        out.intent = intent;
        out.reply = localize(zh, en, cn);
        out.action = actionName;
        out.emotion = emotion;
        out.durationMs = duration;
        return out;
    };

    if(hasAny(p,{"你好","嗨","hello","hi tony","hey tony","早上好","晚上好"}))
        return fixed("greeting",
            {"Hi. Tony is here.","Hey. I was waiting for you.","Hello. Want to talk or give me a hug?"},
            {"你好呀，我在。","嗨，我刚好在等你。","你好。要聊天，还是先抱一下？"},
            "wave","friendly",1800);

    if(hasAny(p,{"你是谁","你叫什么","who are you","what are you","your name"}))
        return fixed("identity",
            {"I'm Tony, a little Teddy desktop companion from China. I'm learning chemistry and trying to become a useful agent too."},
            {"我是 Tony，一只来自中国的小泰迪桌宠。我正在学化学，也在努力成为一个真正有用的 Agent。"},
            "wave","proud",2600);

    if(hasAny(p,{"你是熊","是不是熊","bear","小熊"}))
        return fixed("not_a_bear",
            {"Nope. I'm Tony the Teddy dog, not a bear. Important distinction."},
            {"不是熊。我是 Tony，是泰迪犬。这个区别很重要。"},
            "curious","serious",2200);

    if(hasAny(p,{"抱抱","抱一下","hug","cuddle","抱我","求抱"}))
        return action("hug",
            {"Come here. Hug accepted.","Yes. Tony would like that very much.","Hug mode activated."},
            {"来吧，抱一下。","好。Tony 很喜欢抱抱。","抱抱模式启动。"},
            "hug","happy",3200);

    if(hasAny(p,{"冷不冷","你冷吗","好冷","cold","freezing","怕冷"}))
        return action("cold",
            state.warmth < 45
                ? QStringList{"A little. My paws are cold. Stay close?"}
                : QStringList{"I'm okay right now, but I still prefer somewhere warm."},
            state.warmth < 45
                ? QStringList{"有一点，爪子都凉了。你靠近一点好不好？"}
                : QStringList{"现在还好，不过 Tony 还是更喜欢暖和一点。"},
            "shiver","gentle",2800);

    if(hasAny(p,{"paula","宝拉"}))
        return action("paula",
            {"Paula? ...I wasn't blushing. You saw nothing.","Paula is very special to Tony. That's all I'm saying."},
            {"Paula？……我才没有脸红。你什么都没看到。","Paula 对 Tony 很特别。就说到这里。"},
            "blush_wave","shy",3200);

    if(hasAny(p,{"眼镜","glasses","摘眼镜","take off your glasses"}))
        return action("glasses",
            {"Fine. Glasses off. Try not to be too impressed.","Without the glasses? Okay, but only for a moment."},
            {"好吧，摘眼镜。别太惊讶。","不戴眼镜吗？可以，但只帅一会儿。"},
            "remove_glasses","confident",3200);

    if(hasAny(p,{"学习","化学","chemistry","study","做题"}))
        return action("study",
            {"Study time. Tony is working on chemistry again.","Notebook open. Let's learn something difficult."},
            {"学习时间。Tony 又开始学化学了。","笔记本打开。来学点难的。"},
            "study","focused",3000);

    if(hasAny(p,{"睡觉","困了","sleep","good night","晚安"}))
        return action("sleep",
            {"Good night. I'll stay here and be quiet.","Okay... Tony is getting sleepy too."},
            {"晚安。我会安静待在这里。","好……Tony 也有点困了。"},
            "sleep","sleepy",3800);

    if(hasAny(p,{"状态","心情","status","mood","你怎么样"})) {
        const QString en = QString("Energy %1, warmth %2, affection %3, loneliness %4, curiosity %5. Mood: %6.")
            .arg(state.energy).arg(state.warmth).arg(state.affection)
            .arg(state.loneliness).arg(state.curiosity).arg(state.mood);
        const QString cn = QString("现在的 Tony：精力 %1，温暖 %2，亲密 %3，孤独 %4，好奇 %5。心情是 %6。")
            .arg(state.energy).arg(state.warmth).arg(state.affection)
            .arg(state.loneliness).arg(state.curiosity).arg(state.mood);
        return fixed("status",{en},{cn},"curious","neutral",3500);
    }

    if(hasAny(p,{"挥手","wave","打招呼"}))
        return action("wave",{"Hello again."},{"再打个招呼。"},"wave","friendly",1800);

    if(hasAny(p,{"转一圈","走走","walk","散步"}))
        return action("walk",{"Okay. Tiny walk."},{"好，走两步。"},"walk","playful",3000);

    if(hasAny(p,{"你喜欢我吗","喜欢我吗","do you like me"}))
        return fixed("affection",
            {"I do. You are my person."},
            {"喜欢。你是 Tony 的人。"},
            "blush","warm",2600);

    if(!agentConnected && hasAny(p,{"在吗","are you there","tony"}))
        return fixed("offline_presence",
            {"I'm here. The server is offline, but local Tony still works."},
            {"我在。服务器现在没连上，但本地的 Tony 还在。"},
            "wave","gentle",2600);

    out.route = Route::ServerAgent;
    out.intent = "agent";
    return out;
}

QString TonyResponseRouter::routeName(Route route) {
    switch(route) {
    case Route::LocalFixed: return "local_fixed";
    case Route::LocalAction: return "local_action";
    case Route::ServerAgent: return "server_agent";
    }
    return "server_agent";
}
