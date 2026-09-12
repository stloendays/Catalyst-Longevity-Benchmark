from pathlib import Path

pet_path = Path("bear_agent_pet/client/src/PetWindowV7.cpp")
text = pet_path.read_text(encoding="utf-8-sig")

old_alpha = "if(qAlpha(line[x])>8)"
new_alpha = "if(qAlpha(line[x])>0)"
if text.count(old_alpha) != 1:
    raise SystemExit(f"expected one alpha threshold, found {text.count(old_alpha)}")
text = text.replace(old_alpha, new_alpha, 1)

start_marker = "        // Reserve a real top margin after rotation, not just before it. Hug is the\n"
end_marker = "        p.translate(QPointF(width()/2.0+dx,pivotY));"
start = text.find(start_marker)
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("render safety block not found")
end += len(end_marker)

replacement = '''        // Absolute zero-crop invariant: every non-transparent pixel of Tony must\n        // remain inside the client area after scale, motion and rotation. Animation\n        // offsets are preferences only; when they would cross an edge, the sprite\n        // is shifted inward and, if necessary, uniformly reduced.\n        const qreal topSafety=(action_==Action::Hug) ? 16.0 : 10.0;\n        const qreal sideSafety=10.0;\n        const qreal bottomSafety=34.0; // keep artwork clear of the Tony name plate\n        const qreal safeLeft=sideSafety;\n        const qreal safeRight=width()-sideSafety;\n        const qreal safeTop=topSafety;\n        const qreal safeBottom=height()-bottomSafety;\n\n        const qreal radians=qDegreesToRadians(rotation);\n        const qreal absCos=qAbs(qCos(radians));\n        const qreal absSin=qAbs(qSin(radians));\n        const qreal rotatedUnitW=visibleW*absCos+visibleH*absSin;\n        const qreal rotatedUnitH=visibleH*absCos+visibleW*absSin;\n        const qreal safeWidth=qMax<qreal>(1.0,safeRight-safeLeft);\n        const qreal safeHeight=qMax<qreal>(1.0,safeBottom-safeTop);\n        const qreal zeroCropFit=qMin(\n            safeWidth/qMax<qreal>(1.0,rotatedUnitW),\n            safeHeight/qMax<qreal>(1.0,rotatedUnitH));\n        const qreal fit=qMin(desiredFit,zeroCropFit);\n\n        const qreal rotatedW=rotatedUnitW*fit;\n        const qreal rotatedH=rotatedUnitH*fit;\n        const qreal desiredCenterX=width()/2.0+dx;\n        const qreal desiredCenterY=groundY-visibleH*fit/2.0;\n        const qreal minCenterX=safeLeft+rotatedW/2.0;\n        const qreal maxCenterX=safeRight-rotatedW/2.0;\n        const qreal minCenterY=safeTop+rotatedH/2.0;\n        const qreal maxCenterY=safeBottom-rotatedH/2.0;\n        const qreal centerX=(minCenterX<=maxCenterX)\n            ? qBound(minCenterX,desiredCenterX,maxCenterX)\n            : (safeLeft+safeRight)/2.0;\n        const qreal centerY=(minCenterY<=maxCenterY)\n            ? qBound(minCenterY,desiredCenterY,maxCenterY)\n            : (safeTop+safeBottom)/2.0;\n\n        p.translate(QPointF(centerX,centerY));'''

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

pet_path.write_text(text, encoding="utf-8")

version_path = Path("bear_agent_pet/client/VERSION")
current = version_path.read_text(encoding="utf-8-sig").strip()
if current != "1.0.1":
    raise SystemExit(f"expected Tony 1.0.1 base, got {current}")
version_path.write_text("1.0.2\n", encoding="ascii")

print("TONY_V102_BASE_101=PASS")
print("TONY_V102_ZERO_CROP=PASS")
print("TONY_V102_VERSION=1.0.2")
