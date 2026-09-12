from pathlib import Path

ROOT = Path("bear_agent_pet/client")
HEADER = ROOT / "src/PetWindow.h"
CPP = ROOT / "src/PetWindowV7.cpp"
VERSION = ROOT / "VERSION"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


header = HEADER.read_text(encoding="utf-8-sig")
header = replace_once(
    header,
    "        AdjustGlasses, RemoveGlasses, Wave\n",
    "        AdjustGlasses, RemoveGlasses, Wave,\n"
    "        HeadTilt, Nod, Paw, Hop, Spin, Sniff, Dance\n",
    "Action enum",
)
HEADER.write_text(header, encoding="utf-8")

cpp = CPP.read_text(encoding="utf-8-sig")

cpp = replace_once(
    cpp,
    '    case Action::Wave:return "wave";\n',
    '    case Action::Wave:return "wave";\n'
    '    case Action::HeadTilt:return "curious";\n'
    '    case Action::Nod:return "idle";\n'
    '    case Action::Paw:return "wave";\n'
    '    case Action::Hop:return "celebrate";\n'
    '    case Action::Spin:return "celebrate";\n'
    '    case Action::Sniff:return "curious";\n'
    '    case Action::Dance:return "celebrate";\n',
    "asset mapping",
)

cpp = replace_once(
    cpp,
    '    case Action::Wave:return 4;\n',
    '    case Action::Wave:return 4;\n'
    '    case Action::HeadTilt:return 5;\n'
    '    case Action::Nod:return 4;\n'
    '    case Action::Paw:return 3;\n'
    '    case Action::Hop:return 3;\n'
    '    case Action::Spin:return 2;\n'
    '    case Action::Sniff:return 4;\n'
    '    case Action::Dance:return 3;\n',
    "frame stride",
)

cpp = replace_once(
    cpp,
    '    case Action::Wave: dy=-qAbs(int(3*qSin(t*1.6))); rotation=3.0*qSin(t*1.8); break;\n',
    '    case Action::Wave: dy=-qAbs(int(3*qSin(t*1.6))); rotation=3.0*qSin(t*1.8); break;\n'
    '    case Action::HeadTilt: dx=int(2*qSin(t*.4)); rotation=5.5*qSin(t*.55); scale=1.008; break;\n'
    '    case Action::Nod: dy=int(3*qSin(t*1.4)); scale=1.0+0.008*qSin(t*1.4); break;\n'
    '    case Action::Paw: dy=-qAbs(int(2*qSin(t*1.7))); rotation=4.2*qSin(t*1.7); scale=1.012; break;\n'
    '    case Action::Hop: dy=-qAbs(int(10*qSin(t*1.15))); scale=1.0+0.025*qSin(t*1.15); rotation=1.5*qSin(t*.9); break;\n'
    '    case Action::Spin: rotation=(frame_%40)*9.0; scale=.985; break;\n'
    '    case Action::Sniff: dx=int(3*qSin(t*1.2)); dy=qAbs(int(2*qSin(t*.8))); rotation=-2.0+1.5*qSin(t*.9); break;\n'
    '    case Action::Dance: dx=int(5*qSin(t*1.1)); dy=-qAbs(int(5*qSin(t*1.7))); rotation=5.0*qSin(t*1.1); scale=1.01+0.015*qSin(t*1.4); break;\n',
    "paint motion",
)

cpp = replace_once(
    cpp,
    '    if(n=="wave" || n=="paw_wave" || n=="ear_wiggle" || n=="wake") return Action::Wave;\n'
    '    if(n=="blanket" || n=="warm_hands" || n=="tea") return Action::Shiver;\n',
    '    if(n=="wave" || n=="paw_wave" || n=="ear_wiggle" || n=="wake") return Action::Wave;\n'
    '    if(n=="head_tilt" || n=="tilt_head" || n=="curious_tilt") return Action::HeadTilt;\n'
    '    if(n=="nod" || n=="agree" || n=="yes") return Action::Nod;\n'
    '    if(n=="paw" || n=="high_five" || n=="paw_up") return Action::Paw;\n'
    '    if(n=="hop" || n=="jump" || n=="bounce") return Action::Hop;\n'
    '    if(n=="spin" || n=="twirl") return Action::Spin;\n'
    '    if(n=="sniff" || n=="smell") return Action::Sniff;\n'
    '    if(n=="dance" || n=="happy_dance" || n=="wiggle") return Action::Dance;\n'
    '    if(n=="blanket" || n=="warm_hands" || n=="tea") return Action::Shiver;\n',
    "wire aliases",
)

cpp = replace_once(
    cpp,
    '    using Impulse=TonyBehaviorEngine::Impulse;\n'
    '    const auto impulse=behavior_.chooseIdleImpulse(QTime::currentTime().hour());\n',
    '    // Small, quiet micro-actions make Tony feel alive without turning idle mode\n'
    '    // into a distraction. Flashy actions remain user/agent initiated.\n'
    '    if(QRandomGenerator::global()->bounded(100)<18) {\n'
    '        switch(QRandomGenerator::global()->bounded(4)) {\n'
    '        case 0: emotion_="curious"; setAction(Action::HeadTilt,1500); break;\n'
    '        case 1: emotion_="content"; setAction(Action::Nod,1100); break;\n'
    '        case 2: emotion_="curious"; setAction(Action::Sniff,1700); break;\n'
    '        default: emotion_="friendly"; setAction(Action::Paw,1300); break;\n'
    '        }\n'
    '        scheduleIdleMoment();\n'
    '        return;\n'
    '    }\n\n'
    '    using Impulse=TonyBehaviorEngine::Impulse;\n'
    '    const auto impulse=behavior_.chooseIdleImpulse(QTime::currentTime().hour());\n',
    "idle micro actions",
)

cpp = replace_once(
    cpp,
    '    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";\n',
    '    case Action::AdjustGlasses:return "adjusting glasses"; case Action::RemoveGlasses:return "no-glasses mode"; case Action::Wave:return "waving";\n'
    '    case Action::HeadTilt:return "tilting his head"; case Action::Nod:return "nodding"; case Action::Paw:return "raising a paw";\n'
    '    case Action::Hop:return "hopping"; case Action::Spin:return "spinning"; case Action::Sniff:return "sniffing curiously"; case Action::Dance:return "happy dancing";\n',
    "action names",
)

cpp = replace_once(
    cpp,
    '    auto sleep=actions->addAction(uiText("Sleep","睡觉"));\n',
    '    auto sleep=actions->addAction(uiText("Sleep","睡觉"));\n'
    '    actions->addSeparator();\n'
    '    auto headTilt=actions->addAction(uiText("Tilt head","歪歪头"));\n'
    '    auto nod=actions->addAction(uiText("Nod","点点头"));\n'
    '    auto paw=actions->addAction(uiText("High-five / paw","举爪 / 击掌"));\n'
    '    auto sniff=actions->addAction(uiText("Sniff around","好奇地闻一闻"));\n'
    '    auto hop=actions->addAction(uiText("Hop","开心跳一下"));\n'
    '    auto spin=actions->addAction(uiText("Spin","转一圈"));\n'
    '    auto dance=actions->addAction(uiText("Happy dance","开心舞"));\n',
    "action menu",
)

cpp = replace_once(
    cpp,
    '    else if(chosen==sleep) setAction(Action::Sleep);\n'
    '    else if(chosen==quit) qApp->quit();\n',
    '    else if(chosen==sleep) setAction(Action::Sleep);\n'
    '    else if(chosen==headTilt) { emotion_="curious"; setAction(Action::HeadTilt,1700); }\n'
    '    else if(chosen==nod) { emotion_="content"; setAction(Action::Nod,1300); }\n'
    '    else if(chosen==paw) { emotion_="friendly"; setAction(Action::Paw,1600); }\n'
    '    else if(chosen==sniff) { emotion_="curious"; setAction(Action::Sniff,1900); }\n'
    '    else if(chosen==hop) { emotion_="happy"; setAction(Action::Hop,1700); }\n'
    '    else if(chosen==spin) { emotion_="playful"; setAction(Action::Spin,1800); }\n'
    '    else if(chosen==dance) { emotion_="happy"; setAction(Action::Dance,2600); }\n'
    '    else if(chosen==quit) qApp->quit();\n',
    "action handlers",
)

required = [
    "Action::HeadTilt", "Action::Nod", "Action::Paw", "Action::Hop",
    "Action::Spin", "Action::Sniff", "Action::Dance",
    'n=="high_five"', 'n=="happy_dance"',
    "const qreal zeroCropFit=qMin(",
    "qBound(minCenterX,desiredCenterX,maxCenterX)",
    "if(qAlpha(line[x])>0)",
]
for token in required:
    if token not in cpp and token not in header:
        raise SystemExit(f"missing required action/zero-crop token: {token}")

CPP.write_text(cpp, encoding="utf-8")

version = VERSION.read_text(encoding="utf-8-sig").strip()
if version != "1.0.1":
    raise SystemExit(f"expected base VERSION 1.0.1, got {version}")
VERSION.write_text("1.0.2\n", encoding="utf-8")

print("TONY_V102_ACTIONS_PATCH=PASS")
