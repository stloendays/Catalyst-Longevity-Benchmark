#include "TonyResponseRouter.h"
#include "TonyMemoryStore.h"
#include "TonyResponsePack.h"

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
}

TonyResponseRouter::Decision TonyResponseRouter::resolve(
    const QString &prompt,
    const QString &language,
    bool agentConnected,
    const TonyBehaviorEngine::Snapshot &state) const {

    Decision out;
    out.forwardText=prompt.trimmed();
    QString p=out.forwardText.toLower();

    if(p.startsWith("/agent ")) {
        out.forwardText=out.forwardText.mid(7).trimmed();
        out.intent="forced_agent";
        return out;
    }
    if(p.startsWith("agent:") || p.startsWith(QString::fromUtf8("agent："))) {
        out.forwardText=out.forwardText.mid(6).trimmed();
        out.intent="forced_agent";
        return out;
    }

    auto fixed=[&](const QString &intent,
                   const QStringList &en, const QStringList &cn,
                   const QStringList &actions,
                   const QString &emotion, int duration=2200) {
        out.route=Route::LocalFixed;
        out.intent=intent;
        out.reply=TonyResponsePack::pick(intent,language,en,cn);
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
        out.reply=TonyResponsePack::pick(intent,language,en,cn);
        out.action=pick(actions);
        out.emotion=emotion;
        out.durationMs=duration;
        return out;
    };

    auto sequence=[&](const QString &intent,
                      const QStringList &en, const QStringList &cn,
                      const QStringList &actions, const QStringList &emotions,
                      const QVector<int> &durations,
                      const QString &fallbackEmotion, int bubbleDuration=3200) {
        out.route=Route::LocalAction;
        out.intent=intent;
        out.reply=TonyResponsePack::pick(intent,language,en,cn);
        out.actionSequence=actions;
        out.emotionSequence=emotions;
        out.sequenceDurationsMs=durations;
        out.action=actions.isEmpty() ? QString() : actions.first();
        out.emotion=fallbackEmotion;
        out.durationMs=bubbleDuration;
        return out;
    };

    auto memoryDecision=[&](const QString &intent,
                            const QStringList &en,const QStringList &cn,
                            const QString &memoryAction,const QString &emotion,int duration) {
        out.route=Route::LocalFixed;
        out.intent=intent;
        out.reply=TonyResponsePack::pick(intent,language,en,cn);
        out.action=memoryAction;
        out.emotion=emotion;
        out.durationMs=duration;
        return out;
    };

    const QString rawPrompt=out.forwardText;
    auto extractAfter=[&](const QStringList &prefixes,bool &matched) {
        matched=false;
        for(const auto &prefix:prefixes) {
            if(rawPrompt.startsWith(prefix,Qt::CaseInsensitive)) {
                matched=true;
                QString value=rawPrompt.mid(prefix.size()).trimmed();
                if(value.startsWith(QLatin1Char(':')) || value.startsWith(QChar(0xFF1A)))
                    value=value.mid(1).trimmed();
                return value;
            }
        }
        return QString();
    };

    bool rememberMatched=false;
    const QString rememberFact=extractAfter({
        "please remember ","remember ",QString::fromUtf8("请记住"),QString::fromUtf8("记住")
    },rememberMatched);
    if(rememberMatched) {
        if(rememberFact.isEmpty())
            return memoryDecision("memory_prompt",
                {"Tell me what you want me to remember locally."},
                {QString::fromUtf8("告诉我你想让我在本机记住什么。")},
                "head_tilt","curious",3000);
        TonyMemoryStore::instance().remember(rememberFact);
        return memoryDecision("memory_ack",
            {"Got it. I'll keep that as a local memory."},
            {QString::fromUtf8("记住了。这条只放在本机的 Tony 记忆里。")},
            "nod","content",3000);
    }

    if(p=="/memory clear" || p=="clear memory" || p=="forget all" ||
       p==QString::fromUtf8("清空记忆") || p==QString::fromUtf8("忘掉全部") || p==QString::fromUtf8("忘记全部")) {
        TonyMemoryStore::instance().clear();
        return memoryDecision("memory_clear",
            {"Okay. I cleared Tony's explicit local memories."},
            {QString::fromUtf8("好，Tony 的显式本地记忆已经清空。")},
            "nod","calm",3000);
    }

    bool forgetMatched=false;
    const QString forgetQuery=extractAfter({
        "forget ",QString::fromUtf8("请忘记"),QString::fromUtf8("忘掉"),QString::fromUtf8("忘记")
    },forgetMatched);
    if(forgetMatched) {
        if(forgetQuery.isEmpty())
            return memoryDecision("memory_forget_missing",
                {"Tell me which local memory to forget."},
                {QString::fromUtf8("告诉我你想删掉哪一条本地记忆。")},
                "head_tilt","curious",3000);
        const bool removed=TonyMemoryStore::instance().forgetMatching(forgetQuery);
        return memoryDecision(removed ? "memory_forget" : "memory_forget_missing",
            removed ? QStringList{"Okay. I removed that from local memory."}
                    : QStringList{"I couldn't find a matching local memory."},
            removed ? QStringList{QString::fromUtf8("好，我已经把那条从本地记忆里删掉了。")}
                    : QStringList{QString::fromUtf8("我没有找到匹配的本地记忆。")},
            removed ? "nod" : "head_tilt",removed ? "calm" : "curious",3000);
    }

    if(p=="/memory" || hasAny(p,{
        "what do you remember","show memory","memory list",
        QString::fromUtf8("你记得什么"),QString::fromUtf8("你还记得什么"),QString::fromUtf8("记忆列表")
    })) {
        const auto facts=TonyMemoryStore::instance().facts();
        if(facts.isEmpty())
            return memoryDecision("memory_empty",
                {"I don't have any explicit local memories yet."},
                {QString::fromUtf8("我现在还没有你明确让我保存的本地记忆。")},
                "head_tilt","curious",3200);

        const qsizetype first=facts.size()>6 ? facts.size()-6 : 0;
        const QStringList recent=facts.mid(first);
        out.route=Route::LocalFixed;
        out.intent="memory_recall";
        out.action="head_tilt";
        out.emotion="thoughtful";
        out.durationMs=5200;
        if(language.trimmed().toLower().startsWith("zh")) {
            out.reply=QString::fromUtf8("我记得这些（只在本机）：\n• ")+recent.join(QString::fromUtf8("\n• "));
            if(facts.size()>recent.size())
                out.reply+=QString::fromUtf8("\n……另外还有 %1 条。").arg(facts.size()-recent.size());
        } else {
            out.reply=QStringLiteral("I remember these local notes:\n• ")+recent.join(QStringLiteral("\n• "));
            if(facts.size()>recent.size())
                out.reply+=QStringLiteral("\n...and %1 more.").arg(facts.size()-recent.size());
        }
        return out;
    }

    if(looksTechnical(p)) {
        out.intent="agent_task";
        return out;
    }

    if(hasAny(p,{"/help","帮助","你会做什么","what can you do","help tony"}))
        return fixed("help",
            {"I can react locally, do desktop actions, remember explicit local notes, or send harder requests to the Agent. Use /agent to force the Agent."},
            {"我可以本地回应、做桌面动作、保存你明确要求的本地记忆，也能把复杂任务交给 Agent。输入 /agent 可以强制走 Agent。"},
            {"paw","nod"},"friendly",4000);

    if(hasAny(p,{"早上好","good morning","morning tony"}))
        return sequence("morning",{"Morning. Give me one stretch and I'm ready."},{"早上好。让我先伸个懒腰，马上开工。"},
            {"yawn","stretch","wave"},{"sleepy","content","friendly"},{900,1200,1000},"friendly",3300);

    if(hasAny(p,{"我回来了","回来了","i'm back","im back","back tony"}))
        return sequence("welcome_back",{"You're back. Tony noticed."},{"你回来啦。Tony 有注意到。"},
            {"head_tilt","hop","paw"},{"curious","happy","friendly"},{700,900,1000},"happy",3000);

    if(hasAny(p,{"搞定了","成功了","做完了","we did it","done!","it worked","成功"}))
        return sequence("celebrate_success",{"We did it. Tiny victory dance."},{"搞定。Tony 要跳一个很小的胜利舞。"},
            {"hop","spin","dance"},{"happy","playful","happy"},{700,850,1800},"happy",3800);

    if(hasAny(p,{"我累了","好累","压力好大","难过","stress","stressed","i'm tired","im tired","sad"}))
        return sequence("comfort",{"Come closer. No fixing for a second—just Tony staying here."},{"靠近一点吧。先不解决问题，Tony 就陪你一会儿。"},
            {"head_tilt","ask_hug","nod"},{"gentle","hopeful","gentle"},{800,1300,900},"gentle",3800);

    if(hasAny(p,{"想你了","miss you","i missed you","想tony"}))
        return sequence("miss_you",{"I missed you too. Come here."},{"Tony 也想你。过来抱一下。"},
            {"blush","paw","hug"},{"shy","friendly","happy"},{800,800,1700},"warm",3600);

    if(hasAny(p,{"再见","拜拜","bye","goodbye","see you"}))
        return sequence("goodbye",{"Bye. I'll keep your spot on the desktop."},{"拜拜。桌面上的位置我给你留着。"},
            {"paw","wave","nod"},{"friendly","warm","content"},{700,1100,700},"warm",3000);

    if(hasAny(p,{"你好","嗨","hello","hi tony","hey tony","晚上好"}))
        return fixed("greeting",{"Hi. Tony is here.","Hey. I was waiting for you.","Hello. Want a paw?"},
            {"你好呀，我在。","嗨，我刚好在等你。","你好。要不要先击个掌？"},{"paw","wave","head_tilt"},"friendly",1700);

    if(hasAny(p,{"你是谁","你叫什么","who are you","what are you","your name"}))
        return fixed("identity",{"I'm Tony, a little Teddy dog from China. I'm learning chemistry and becoming a desktop agent."},
            {"我是 Tony，一只来自中国的小泰迪犬。我在学化学，也在努力成为真正的桌面 Agent。"},{"nod","paw"},"proud",2600);

    if(hasAny(p,{"你是熊","是不是熊","are you a bear","小熊"}))
        return fixed("not_a_bear",{"Nope. Tony is a Teddy dog, not a bear. Important distinction."},
            {"不是熊。Tony 是泰迪犬。这个区别很重要。"},{"head_tilt","nod"},"serious",2200);

    if(hasAny(p,{"抱抱","抱一下","hug","cuddle","抱我","求抱"}))
        return sequence("hug",{"Come here. Hug accepted.","Yes. Tony would like that very much.","Hug mode activated."},
            {"来吧，抱一下。","好。Tony 很喜欢抱抱。","抱抱模式启动。"},{"ask_hug","hug","nod"},{"hopeful","happy","content"},{750,1700,750},"happy",3600);

    if(hasAny(p,{"冷不冷","你冷吗","好冷","cold","freezing","怕冷"}))
        return action("cold",
            state.warmth<45 ? QStringList{"A little. My paws are cold. Stay close?"} : QStringList{"I'm okay now, but I still prefer somewhere warm."},
            state.warmth<45 ? QStringList{"有一点，爪子都凉了。你靠近一点好不好？"} : QStringList{"现在还好，不过 Tony 还是更喜欢暖和一点。"},
            {"shiver"},"gentle",2800);

    if(hasAny(p,{"paula","宝拉"}))
        return sequence("paula",{"Paula? ...I wasn't blushing. You saw nothing.","Paula is very special to Tony. That's all I'm saying."},
            {"Paula？……我才没有脸红。你什么都没看到。","Paula 对 Tony 很特别。就说到这里。"},
            {"head_tilt","blush_wave","adjust_glasses"},{"curious","shy","focused"},{650,1600,900},"shy",3900);

    if(hasAny(p,{"摘眼镜","不戴眼镜","take off your glasses","without glasses"}))
        return sequence("remove_glasses",{"Fine. Glasses off. Try not to be too impressed.","Only for a moment. Handsome mode is dangerous."},
            {"好吧，摘眼镜。别太惊讶。","只帅一会儿。这个模式有点危险。"},
            {"adjust_glasses","remove_glasses","hop"},{"focused","confident","happy"},{650,1600,700},"confident",3800);

    if(hasAny(p,{"眼镜","glasses"}))
        return action("glasses",{"Still here. Let me fix my glasses first.","Glasses adjusted. Much better."},
            {"在呢，先让我扶一下眼镜。","眼镜扶好了。这样顺眼多了。"},{"adjust_glasses"},"focused",1900);

    if(hasAny(p,{"学习一下","开始学习","study time","study chemistry","学化学"}))
        return sequence("study",{"Study time. Tony has the notebook ready.","Okay. Chemistry mode on."},
            {"学习时间。Tony 已经把笔记本打开了。","好，化学学习模式启动。"},
            {"adjust_glasses","study","nod"},{"focused","focused","content"},{650,1800,700},"focused",3600);

    if(hasAny(p,{"睡觉","困了","sleep","good night","晚安"}))
        return action("sleep",{"Good night. I'll stay here quietly.","Okay... Tony is sleepy too."},
            {"晚安。我会安静待在这里。","好……Tony 也有点困了。"},{"sleep","yawn"},"sleepy",3800);

    if(hasAny(p,{"状态","心情","status","mood","你怎么样"})) {
        const QString en=QString("Energy %1, warmth %2, affection %3, loneliness %4, curiosity %5. Mood: %6.")
            .arg(state.energy).arg(state.warmth).arg(state.affection).arg(state.loneliness).arg(state.curiosity).arg(state.mood);
        const QString cn=QString("现在的 Tony：精力 %1，温暖 %2，亲密 %3，孤独 %4，好奇 %5。心情是 %6。")
            .arg(state.energy).arg(state.warmth).arg(state.affection).arg(state.loneliness).arg(state.curiosity).arg(state.mood);
        return fixed("status",{en},{cn},{"head_tilt","nod"},"neutral",3500);
    }

    if(hasAny(p,{"谢谢","thank you","thanks"}))
        return fixed("thanks",{"Anytime. Paw?","You're welcome. Tony is staying right here."},{"随时。击个掌？","不用谢，Tony 就在这里。"},{"paw","nod"},"warm",1800);

    if(hasAny(p,{"对不起","抱歉","sorry"}))
        return fixed("sorry",{"We're okay. Come here.","No problem. Tony isn't keeping score."},{"没事。过来吧。","没关系，Tony 不记这种账。"},{"nod","hug"},"gentle",2400);

    if(hasAny(p,{"无聊","boring","bored"}))
        return sequence("bored",{"Bored? I can fix that.","Then Tony votes for a tiny dance."},{"无聊？那我来想办法。","那 Tony 投票：跳个小舞。"},
            {"head_tilt","spin","dance"},{"curious","playful","happy"},{650,850,1700},"playful",3600);

    if(hasAny(p,{"加油","鼓励我","cheer me up","wish me luck","good luck"}))
        return sequence("encourage",{"You can do it. Tony is on your side.","Go get it. High-five first."},{"你可以的。Tony 站你这边。","去吧，先击个掌。"},
            {"nod","paw","hop"},{"supportive","friendly","happy"},{650,900,750},"supportive",3100);

    if(hasAny(p,{"你好帅","真帅","可爱","cute","handsome","good boy"}))
        return sequence("compliment",{"I know... but hearing it still works.","Careful. Compliments may cause dancing."},{"我知道……但听到还是会开心。","小心，夸多了 Tony 会跳舞。"},
            {"blush","hop","dance"},{"shy","happy","happy"},{650,750,1500},"happy",3500);

    if(hasAny(p,{"饿不饿","吃饭","零食","food","snack","hungry"}))
        return sequence("food",{"Did someone say snack?","I should investigate that smell."},{"刚才是不是有人说零食？","我得去闻闻是什么味道。"},
            {"head_tilt","sniff","hop"},{"curious","curious","happy"},{550,1200,650},"curious",3000);

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
        return fixed("affection",{"I do. I like staying close to you."},{"喜欢。我很喜欢待在你旁边。"},{"blush","nod"},"warm",2500);

    if(!agentConnected && hasAny(p,{"在吗","are you there","tony"}))
        return fixed("offline_presence",{"I'm here. The server is offline, but local Tony still works."},{"我在。服务器现在没连上，但本地的 Tony 还在。"},{"paw","wave"},"gentle",2400);

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
