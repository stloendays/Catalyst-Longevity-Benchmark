#!/usr/bin/env python3
from pathlib import Path

path = Path('bear_agent_pet/client/src/PetWindowV7.cpp')
text = path.read_text(encoding='utf-8')

old = '''    // Idle may use only the short face-blink sequence. Do not loop the legacy
    // full-body animation sets: their crops/proportions vary and caused Tony to
    // appear to lose ears, feet or arms between frames.
    if(action==Action::Idle) {
        auto framesIt=animationAssets_.constFind("idle");
        auto idleIt=stateAssets_.constFind("idle");
        if(idleBlinking_ && framesIt!=animationAssets_.constEnd() && framesIt.value().size()>=2) {
            // frame_03 is intentionally excluded: it is the known malformed/
            // inconsistent blink frame. -1 means use the approved static idle pose.
            static constexpr int blinkSequence[] = {-1, 1, 1, -1, -1};
            const int sequenceSize=static_cast<int>(sizeof(blinkSequence)/sizeof(blinkSequence[0]));
            const int step=qBound(0,idleBlinkTick_,sequenceSize-1);
            const int index=blinkSequence[step];
            if(index>=0) return &framesIt.value().at(index);
        }
        if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
        if(framesIt!=animationAssets_.constEnd() && !framesIt.value().isEmpty()) return &framesIt.value().first();
    }
'''

new = '''    // Never use the historical assets/animations/idle frames at runtime.
    // Runtime Windows QA proved that frame_02 as well as frame_03 contains
    // screenshot background/text and incomplete character framing. Keep the
    // approved full state image as the only Idle artwork; idleBlinking_ remains
    // a subtle motion-only micro-expression until a clean same-canvas blink set
    // is explicitly authored and visually approved.
    if(action==Action::Idle) {
        auto idleIt=stateAssets_.constFind("idle");
        if(idleIt!=stateAssets_.constEnd() && !idleIt.value().isNull()) return &idleIt.value();
    }
'''

if text.count(old) != 1:
    raise SystemExit('Idle block changed upstream; refusing blind patch')
text = text.replace(old, new, 1)

# Full-exposure and 0.9.6 behavior invariants must survive this tiny fix.
required = [
    'setFixedSize(280,250);',
    'QTimer::singleShot(2200,this,[this]',
    'const qreal topSafety=(action_==Action::Hug) ? 16.0 : 8.0;',
    'p.drawPixmap(target,*sprite,QRectF(0,0,sprite->width(),sprite->height()));',
]
for needle in required:
    if needle not in text:
        raise SystemExit(f'Expected 0.9.6 invariant missing: {needle}')

if 'animationAssets_.constFind("idle")' in text:
    raise SystemExit('Legacy idle animation lookup still reachable after patch')

path.write_text(text, encoding='utf-8')
print('TONY_V097_IDLE_FIX=PASS')
