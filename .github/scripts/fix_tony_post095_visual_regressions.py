#!/usr/bin/env python3
from pathlib import Path

path = Path('bear_agent_pet/client/src/PetWindowV7.cpp')
text = path.read_text(encoding='utf-8')

old_idle = '''    // Idle may use only the short face-blink sequence. Do not loop the legacy
    // full-body animation sets: their crops/proportions vary and caused Tony to
    // appear to lose ears, feet or arms between frames.
    if(action==Action::Idle) {
        auto framesIt=animationAssets_.constFind("idle");
        if(idleBlinking_ && framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) {
            static constexpr int blinkSequence[] = {0, 1, 2, 2, 1, 0, 0};
            const int sequenceSize=static_cast<int>(sizeof(blinkSequence)/sizeof(blinkSequence[0]));
            const int step=qBound(0,idleBlinkTick_,sequenceSize-1);
            const int index=blinkSequence[step] % framesIt.value().size();
            return &framesIt.value().at(index);
        }
        auto idleIt=stateAssets_.constFind("idle");
        if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
    }
'''
new_idle = '''    // Never consume the legacy idle frame directory at runtime. Those files are
    // historical screenshot-derived assets with inconsistent crops/backgrounds;
    // a blink must never replace the complete approved Tony artwork with one of
    // those partial frames. idleBlinking_ is therefore motion-only until a clean,
    // same-canvas blink set is explicitly authored and approved.
    if(action==Action::Idle) {
        auto idleIt=stateAssets_.constFind("idle");
        if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
    }
'''
if old_idle not in text:
    raise SystemExit('idle runtime block changed upstream; refusing blind patch')
text = text.replace(old_idle, new_idle, 1)

old_hint = '''    } else {
        tray_.setToolTip("Tony · not paired");
        if(visualTestAction.isEmpty())
            showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);
    }
'''
new_hint = '''    } else {
        tray_.setToolTip("Tony · not paired");
        if(visualTestAction.isEmpty()) {
            // Do not start the bubble's 6.5 s lifetime while main() is still in
            // cold-start work. Queue it onto the event loop so slow Windows hosts
            // actually get the full visible hint after Tony can paint.
            QTimer::singleShot(650,this,[this]{
                QSettings current;
                const QString stored=unprotectSecret(current.value("agent/token","").toString());
                if(stored.isEmpty() && !agent_.connected())
                    showBubble(uiText("Hi Paula. Right-click me and choose Connect to Tony.","嗨 Paula。右键点我，然后选择“连接 Tony”。"),6500);
            });
        }
    }
'''
if old_hint not in text:
    raise SystemExit('first-run hint block changed upstream; refusing blind patch')
text = text.replace(old_hint, new_hint, 1)

old_hug = '    case Action::Hug: scale=1.055+0.02*qSin(t*.8); dy=-3; rotation=1.5*qSin(t*.65); break;\n'
new_hug = '    case Action::Hug: scale=1.004+0.004*qSin(t*.8); dy=-2; rotation=.8*qSin(t*.65); break;\n'
if old_hug not in text:
    raise SystemExit('hug motion block changed upstream; refusing blind patch')
text = text.replace(old_hug, new_hug, 1)

old_cap = '        const qreal maxMotionScale=(action_==Action::Hug) ? 1.035 : 1.02;\n'
new_cap = '        const qreal maxMotionScale=(action_==Action::Hug) ? 1.012 : 1.02;\n'
if old_cap not in text:
    raise SystemExit('hug scale cap changed upstream; refusing blind patch')
text = text.replace(old_cap, new_cap, 1)

# Guardrails: full-source rendering and 280 px host must remain intact.
required = [
    'setFixedSize(280,250);',
    'p.drawPixmap(target,*sprite,QRectF(0,0,sprite->width(),sprite->height()));',
    'const qreal maxVisibleW=252.0;',
    'const qreal maxVisibleH=192.0;',
]
for needle in required:
    if needle not in text:
        raise SystemExit(f'full-exposure invariant missing: {needle}')
if 'animationAssets_.constFind("idle")' in text:
    raise SystemExit('legacy idle animation lookup still reachable')

path.write_text(text, encoding='utf-8')
print('TONY_POST095_VISUAL_PATCH=PASS')
