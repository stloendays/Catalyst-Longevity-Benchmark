from pathlib import Path

path = Path("bear_agent_pet/client/src/PetWindowV7.cpp")
text = path.read_text(encoding="utf-8-sig")

alpha_old = "if(qAlpha(line[x])>8)"
alpha_new = "if(qAlpha(line[x])>0)"
if text.count(alpha_old) != 1:
    raise SystemExit(f"expected one alpha-bound threshold, found {text.count(alpha_old)}")
text = text.replace(alpha_old, alpha_new, 1)

start_marker = "        // Reserve a real top margin after rotation, not just before it. Hug is the\n"
end_marker = "        p.translate(QPointF(width()/2.0+dx,pivotY));"
start = text.find(start_marker)
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("Tony rendering safety block was not found")
end += len(end_marker)

replacement = r'''        // Zero-crop invariant: after scaling and rotation, every visible alpha
        // pixel must remain inside a safe client rectangle. Animation offsets are
        // preferences only; clamp the rendered center inward before Qt can clip.
        const qreal topSafety=(action_==Action::Hug) ? 16.0 : 10.0;
        const qreal sideSafety=10.0;
        const qreal bottomSafety=34.0;
        const qreal safeLeft=sideSafety;
        const qreal safeRight=width()-sideSafety;
        const qreal safeTop=topSafety;
        const qreal safeBottom=height()-bottomSafety;

        const qreal radians=qDegreesToRadians(rotation);
        const qreal absCos=qAbs(qCos(radians));
        const qreal absSin=qAbs(qSin(radians));
        const qreal rotatedUnitW=visibleW*absCos+visibleH*absSin;
        const qreal rotatedUnitH=visibleH*absCos+visibleW*absSin;
        const qreal safeWidth=qMax<qreal>(1.0,safeRight-safeLeft);
        const qreal safeHeight=qMax<qreal>(1.0,safeBottom-safeTop);
        const qreal zeroCropFit=qMin(
            safeWidth/qMax<qreal>(1.0,rotatedUnitW),
            safeHeight/qMax<qreal>(1.0,rotatedUnitH));
        const qreal fit=qMin(desiredFit,zeroCropFit);

        const qreal rotatedW=rotatedUnitW*fit;
        const qreal rotatedH=rotatedUnitH*fit;
        const qreal desiredCenterX=width()/2.0+dx;
        const qreal desiredCenterY=groundY-visibleH*fit/2.0;
        const qreal minCenterX=safeLeft+rotatedW/2.0;
        const qreal maxCenterX=safeRight-rotatedW/2.0;
        const qreal minCenterY=safeTop+rotatedH/2.0;
        const qreal maxCenterY=safeBottom-rotatedH/2.0;
        const qreal centerX=(minCenterX<=maxCenterX)
            ? qBound(minCenterX,desiredCenterX,maxCenterX)
            : (safeLeft+safeRight)/2.0;
        const qreal centerY=(minCenterY<=maxCenterY)
            ? qBound(minCenterY,desiredCenterY,maxCenterY)
            : (safeTop+safeBottom)/2.0;

        p.translate(QPointF(centerX,centerY));'''
text = text[:start] + replacement + text[end:]

required = [
    "if(qAlpha(line[x])>0)",
    "const qreal zeroCropFit=qMin(",
    "qBound(minCenterX,desiredCenterX,maxCenterX)",
    "qBound(minCenterY,desiredCenterY,maxCenterY)",
    "QRectF(0,0,sprite->width(),sprite->height())",
]
for token in required:
    if token not in text:
        raise SystemExit(f"missing zero-crop invariant: {token}")

forbidden = [
    "if(qAlpha(line[x])>8)",
    "p.translate(QPointF(width()/2.0+dx,pivotY));",
    "source=QRect(QPoint(minX,minY)",
]
for token in forbidden:
    if token in text:
        raise SystemExit(f"legacy crop risk remains: {token}")

path.write_text(text, encoding="utf-8")
print("TONY_RELEASE_ZERO_CROP_PATCH=PASS")
