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

bool looksTechnical(const QString &text) {
    return hasAny(text, {
        "解释", "为什么", "怎么做", "分析", "计算", "代码", "编译", "报错", "论文", "公式",
        "what is", "why", "how do", "explain", "analyze", "calculate", "code", "compile", "error",
        "dft", "scf", "vasp", "python", "c++", "qt", "github", "server", "api", "mcp", "agent"
    });
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
    out.forwardText=prompt.trimmed();
    QString p=out.forwardText.toLower();
    const bool zh=language.trimmed().toLower().startsWith("zh");

    if(p.startsWith("/agent ")) {
        out.forwardText=out.forwardText.mid(7).trimmed();
        out.intent="forced_agent";
        return out;
    }
    if(p.startsWith("agent:")) {
        out.forwardText=out.forwardText.mid(6).trimmed();
        out.intent="forced_agent";
        return out;
    }

    // Questions that need reasoning, tools or domain knowledge must never be
    // swallowed by a cute fixed reply just because they contain words such as
    // chemistry or GitHub.
    if(looksTechnical(p)) {
        out.intent="agent_task";
        return out;
    }

    auto fixed=[&](const QString &intent,
                   const QStringList &en, const QStringList &cn,
                   const QStringList &actions,
                   const QString &emotion, int duration=2200) {
        out.route=Route::LocalFixed;
        out.intent=intent;
        out.reply=localize(zh,en,cn);
        out.action=pick(actions);
        out.emotion=emotion;
        out.durationMs=duration;
        return out;
    };

    auto action=[&](const QString &intent,
                    const QStringList &en, const QStringList &cn,
                    const QStringList &actions,
                    const QString &emotion, int duration=2600) {
        out.route=Route::LocalAction;
        out.intent=intent;
        out.reply=localize(zh,en,cn);
        out.action=pick(actions);
        out.emotion=emotion;
        out.durationMs=duration;
        return out;
    };

    if(hasAny(p,{"你好","嗨","hello","hi tony","hey tony","早上好","晚上好"}))
        return fixed("greeting",
            {"Hi. Tony is here.","Hey. I was waiting for you.","Hello. Want a paw?"},
            {"你好呀，我在。","嗨，我刚好在等你。","你好。要不要先击个掌？"},
            {"paw","wave","head_tilt"},"friendly",1700);

    if(hasAny(p,{"你是谁","你叫什么","who are you","what are you","your name"}))
        return fixed("identity",
            {"I'm Tony, a little Teddy dog from China. I'm learning chemistry and becoming a desktop agent."},
            {"我是 Tony，一只来自中国的小泰迪犬。我在学化学，也在努力成为真正的桌面 Agent。"},
            {"nod","paw"},"proud",2600);

    if(hasAny(p,{"你是熊","是不是熊","are you a bear","小熊"}))
        return fixed("not_a_bear",
            {"Nope. Tony is a Teddy dog, not a bear. Important distinction."},
            {"不是熊。Tony 是泰迪犬。这个区别很重要。"},
            {"head_tilt","nod"},"serious",2200);

    if(hasAny(p,{"抱抱","抱一下","hug","cuddle","抱我","求抱"}))
        return action("hug",
            {"Come here. Hug accepted.","Yes. Tony would like that very much.","Hug mode activated."},
            {"来吧，抱一下。","好。Tony 很喜欢抱抱。","抱抱模式启动。"},
            {"hug"},"happy",3200);

    if(hasAny(p,{"冷不冷","你冷吗","好冷","cold","freezing","怕冷"}))
        return action("cold",
            state.warmth<45 ? QStringList{"A little. My paws are cold. Stay close?"}
                            : QStringList{"I'm okay now, but I still prefer somewhere warm."},
            state.warmth<45 ? QStringList{"有一点，爪子都凉了。你靠近一点好不好？"}
                            : QStringList{"现在还好，不过 Tony 还是更喜欢暖和一点。"},
            {"shiver"},"gentle",2800);

    if(hasAny(p,{"paula","宝拉"}))
        return action("paula",
            {"Paula? ...I wasn't blushing. You saw nothing.","Paula is very special to Tony. That's all I'm saying."},
            {"Paula？……我才没有脸红。你什么都没看到。","Paula 对 Tony 很特别。就说到这里。"},
            {"blush_wave","blush"},"shy",3200);

    if(hasAny(p,{"摘眼镜","不戴眼镜","take off your glasses","without glasses"}))
        return action("remove_glasses",
            {"Fine. Glasses off. Try not to be too impressed.","Only for a moment. Handsome mode is dangerous."},
            {"好吧，摘眼镜。别太惊讶。","只帅一会儿。这个模式有点危险。"},
            {"remove_glasses"},"confident",3200);

    if(hasAny(p,{"眼镜","glasses"}))
        return action("glasses",
            {"Still here. Let me fix my glasses first.","Glasses adjusted. Much better."},
            {"在呢，先让我扶一下眼镜。","眼镜扶好了。这样顺眼多了。"},
            {"adjust_glasses"},"focused",1900);

    if(hasAny(p,{"学习一下","开始学习","study time","study chemistry","学化学"}))
        return action("study",
            {"Study time. Tony has the notebook ready.","Okay. Chemistry mode on."},
            {"学习时间。Tony 已经把笔记本打开了。","好，化学学习模式启动。"},
            {"study","nod"},"focused",3000);

    if(hasAny(p,{"睡觉","困了","sleep","good night","晚安"}))
        return action("sleep",
            {"Good night. I'll stay here quietly.","Okay... Tony is sleepy too."},
            {"晚安。我会安静待在这里。","好……Tony 也有点困了。"},
            {"sleep","yawn"},"sleepy",3800);

    if(hasAny(p,{"状态","心情","status","mood","你怎么样"})) {
        const QString en=QString("Energy %1, warmth %2, affection %3, loneliness %4, curiosity %5. Mood: %6.")
            .arg(state.energy).arg(state.warmth).arg(state.affection)
            .arg(state.loneliness).arg(state.curiosity).arg(state.mood);
        const QString cn=QString("现在的 Tony：精力 %1，温暖 %2，亲密 %3，孤独 %4，好奇 %5。心情是 %6。")
            .arg(state.energy).arg(state.warmth).arg(state.affection)
            .arg(state.loneliness).arg(state.curiosity).arg(state.mood);
        return fixed("status",{en},{cn},{"head_tilt","nod"},"neutral",3500);
    }

    if(hasAny(p,{"谢谢","thank you","thanks"}))
        return fixed("thanks",
            {"Anytime. Paw?","You're welcome. Tony is staying right here."},
            {"随时。击个掌？","不用谢，Tony 就在这里。"},
            {"paw","nod"},"warm",1800);

    if(hasAny(p,{"对不起","抱歉","sorry"}))
        return fixed("sorry",
            {"We're okay. Come here.","No problem. Tony isn't keeping score."},
            {"没事。过来吧。","没关系，Tony 不记这种账。"},
            {"nod","hug"},"gentle",2400);

    if(hasAny(p,{"无聊","boring","bored"}))
        return action("bored",
            {"Bored? I can fix that.","Then Tony votes for a tiny dance."},
            {"无聊？那我来想办法。","那 Tony 投票：跳个小舞。"},
            {"dance","spin","hop"},"playful",2600);

    if(hasAny(p,{"加油","鼓励我","cheer me up","wish me luck","good luck"}))
        return action("encourage",
            {"You can do it. Tony is on your side.","Go get it. High-five first."},
            {"你可以的。Tony 站你这边。","去吧，先击个掌。"},
            {"paw","hop","nod"},"supportive",2200);

    if(hasAny(p,{"你好帅","真帅","可爱","cute","handsome","good boy"}))
        return action("compliment",
            {"I know... but hearing it still works.","Careful. Compliments may cause dancing."},
            {"我知道……但听到还是会开心。","小心，夸多了 Tony 会跳舞。"},
            {"hop","dance","blush"},"happy",2400);

    if(hasAny(p,{"饿不饿","吃饭","零食","food","snack","hungry"}))
        return action("food",
            {"Did someone say snack?","I should investigate that smell."},
            {"刚才是不是有人说零食？","我得去闻闻是什么味道。"},
            {"sniff","head_tilt"},"curious",2100);

    if(hasAny(p,{"击掌","high five","give me a paw","爪爪"}))
        return action("high_five",{"Paw!"},{"爪爪！"},{"paw"},"friendly",1600);

    if(hasAny(p,{"点头","nod","同意吗","agree"}))
        return action("nod",{"Yep."},{"嗯。"},{"nod"},"content",1300);

    if(hasAny(p,{"转一圈","spin","twirl"}))
        return action("spin",{"Watch this."},{"看我的。"},{"spin"},"playful",1900);

    if(hasAny(p,{"跳舞","dance"}))
        return action("dance",{"Fine. One tiny dance."},{"好吧，只跳一小段。"},{"dance"},"happy",2700);

    if(hasAny(p,{"散步","走走","walk"}))
        return action("walk",{"Okay. Tiny walk."},{"好，走两步。"},{"walk"},"playful",3000);

    if(hasAny(p,{"喜欢我吗","你喜欢我吗","do you like me"}))
        return fixed("affection",
            {"I do. I like staying close to you."},
            {"喜欢。我很喜欢待在你旁边。"},
            {"blush","nod"},"warm",2500);

    if(!agentConnected && hasAny(p,{"在吗","are you there","tony"}))
        return fixed("offline_presence",
            {"I'm here. The server is offline, but local Tony still works."},
            {"我在。服务器现在没连上，但本地的 Tony 还在。"},
            {"paw","wave"},"gentle",2400);

    out.intent="agent";
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
