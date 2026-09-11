from pathlib import Path

p = Path('bear_agent_pet/client/src/PetWindowV7.cpp')
s = p.read_text(encoding='utf-8')


def replace_once(old: str, new: str) -> None:
    global s
    count = s.count(old)
    if count != 1:
        raise SystemExit(f'expected exactly one match, found {count}: {old[:100]!r}')
    s = s.replace(old, new, 1)

# The third legacy idle frame is the known bad blink frame. Keep the approved
# idle state as the rest pose and use only frame 02 for the eyelid closure.
replace_once(
'''    if(action==Action::Idle) {
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
''',
'''    if(action==Action::Idle) {
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
''',
)

replace_once(
'''    if(action_==Action::Idle && idleBlinking_) {
        ++idleBlinkTick_;
        if(idleBlinkTick_>=7) {
''',
'''    if(action_==Action::Idle && idleBlinking_) {
        ++idleBlinkTick_;
        if(idleBlinkTick_>=5) {
''',
)

replace_once(
'''void PetWindow::scheduleBlink(){
    if(blinkTimer_.isActive()) return;
    // Natural but quiet: roughly one blink every 5-11 seconds.
    blinkTimer_.start(QRandomGenerator::global()->bounded(5000,11001));
}

void PetWindow::scheduleIdleMoment(){
    // Check often enough to feel alive, but the behavior engine returns None most of the time.
    idleTimer_.start(QRandomGenerator::global()->bounded(22000,55001));
}
''',
'''void PetWindow::scheduleBlink(){
    if(blinkTimer_.isActive()) return;
    // Quiet idle cadence: avoid repetitive blinking while Tony is just sitting.
    blinkTimer_.start(QRandomGenerator::global()->bounded(7500,15001));
}

void PetWindow::scheduleIdleMoment(){
    // First/autonomous personality prompts must not fire immediately after launch.
    // Keep the proven calm cadence from the later V0.9 line: 140-320 seconds.
    idleTimer_.start(QRandomGenerator::global()->bounded(140000,320001));
}
''',
)

# Delay functional first-run onboarding until the pet has been visible long enough
# for the bubble to be perceived as attached to Tony, rather than appearing during startup.
replace_once(
'''    } else {
        tray_.setToolTip("Tony · preparing connection code");
        if(visualTestAction.isEmpty()) {
            showBubble(uiText("Creating a secure connection code…","正在生成安全连接码…"),0);
            QTimer::singleShot(700,this,&PetWindow::startAutomaticPairing);
        }
    }
''',
'''    } else {
        tray_.setToolTip("Tony · preparing connection code");
        if(visualTestAction.isEmpty()) {
            QTimer::singleShot(2200,this,[this]{
                if(!agent_.connected() && pairingCode_.isEmpty() && !pairingRequestActive_)
                    startAutomaticPairing();
            });
        }
    }
''',
)

replace_once(
'''    connect(&agent_, &AgentClient::pairingCodeReady, this,
            [this](const QString &code, qint64 expiresAt){
        pairingCode_=code;
''',
'''    connect(&agent_, &AgentClient::pairingCodeReady, this,
            [this](const QString &code, qint64 expiresAt){
        pairingRequestActive_=false;
        pairingCode_=code;
''',
)

replace_once(
'''        s.setValue("agent/device_id",deviceId);
        pairingCode_.clear();
''',
'''        s.setValue("agent/device_id",deviceId);
        pairingRequestActive_=false;
        pairingCode_.clear();
''',
)

replace_once(
'''    connect(&agent_, &AgentClient::pairingFailed, this, [this](const QString &text){
        pairingCode_.clear();
''',
'''    connect(&agent_, &AgentClient::pairingFailed, this, [this](const QString &text){
        pairingRequestActive_=false;
        pairingCode_.clear();
''',
)

replace_once(
'''void PetWindow::startAutomaticPairing(){
    QSettings s;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
''',
'''void PetWindow::startAutomaticPairing(){
    const qint64 now=QDateTime::currentSecsSinceEpoch();
    if(!pairingCode_.isEmpty() && pairingCodeExpiresAt_>now+2) {
        showCurrentPairingCode(false);
        return;
    }
    if(pairingRequestActive_) return;

    QSettings s;
    const QUrl endpoint(s.value("agent/public_url",defaultPublicEndpoint()).toString());
''',
)

replace_once(
'''    pairingCode_.clear();
    pairingCodeExpiresAt_=0;
    emotion_="curious";
''',
'''    pairingCode_.clear();
    pairingCodeExpiresAt_=0;
    pairingRequestActive_=true;
    emotion_="curious";
''',
)

# Compute a rotation-aware alpha-bounds safety cap. The previous Hug geometry left
# only ~3 px above the visible subject before rotation, so ±1.5 degrees could clip it.
replace_once(
'''        const qreal maxMotionScale=(action_==Action::Hug) ? 1.035 : 1.02;
        const qreal safeMotionScale=qBound<qreal>(0.97,scale,maxMotionScale);
        const qreal maxVisibleW=252.0;
        const qreal maxVisibleH=192.0;
        const qreal baseFit=qMin(maxVisibleW/visibleW,maxVisibleH/visibleH);
        const qreal fit=baseFit*safeMotionScale;

        const qreal visibleCx=(minX+maxX+1)/2.0;
        const qreal visibleCy=(minY+maxY+1)/2.0;
        const qreal groundY=205.0+dy;
        const qreal pivotY=groundY-visibleH*fit/2.0;
''',
'''        const qreal maxMotionScale=(action_==Action::Hug) ? 1.035 : 1.02;
        const qreal safeMotionScale=qBound<qreal>(0.97,scale,maxMotionScale);
        const qreal maxVisibleW=252.0;
        const qreal maxVisibleH=192.0;
        const qreal desiredFit=qMin(maxVisibleW/visibleW,maxVisibleH/visibleH)*safeMotionScale;

        const qreal visibleCx=(minX+maxX+1)/2.0;
        const qreal visibleCy=(minY+maxY+1)/2.0;
        const qreal groundY=205.0+dy;

        // Reserve a real top margin after rotation, not just before it. Hug is the
        // widest/most expanded pose and gets 16 px; other poses keep at least 8 px.
        const qreal topSafety=(action_==Action::Hug) ? 16.0 : 8.0;
        const qreal sideSafety=14.0;
        const qreal radians=qDegreesToRadians(rotation);
        const qreal absCos=qAbs(qCos(radians));
        const qreal absSin=qAbs(qSin(radians));
        const qreal rotatedUnitW=visibleW*absCos+visibleH*absSin;
        const qreal rotatedUnitH=visibleH*absCos+visibleW*absSin;
        const qreal safetyFit=qMin(
            qMax<qreal>(1.0,width()-2.0*sideSafety)/qMax<qreal>(1.0,rotatedUnitW),
            qMax<qreal>(1.0,groundY-topSafety)/qMax<qreal>(1.0,rotatedUnitH));
        const qreal fit=qMin(desiredFit,safetyFit);
        const qreal pivotY=groundY-visibleH*fit/2.0;
''',
)

p.write_text(s, encoding='utf-8')
print('TONY_V096_VISUAL_SAFETY_PATCH=PASS')
