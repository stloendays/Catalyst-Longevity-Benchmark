from pathlib import Path

path = Path('teddy_agent_pet/client/src/PetWindowV7.cpp')
text = path.read_text(encoding='utf-8')
old = '''    const QRect full=screen->geometry();
    constexpr int tolerance=8;
    const bool isFullscreen=
        windowRect.left()<=full.left()+tolerance &&
        windowRect.top()<=full.top()+tolerance &&
        windowRect.right()-1>=full.right()-tolerance &&
        windowRect.bottom()-1>=full.bottom()-tolerance;
'''
new = '''    const QRect full=screen->geometry();
    constexpr int tolerance=8;
    const LONG_PTR style=GetWindowLongPtrW(foreground,GWL_STYLE);
    const bool borderless=(style & WS_CAPTION)==0 && (style & WS_THICKFRAME)==0;
    const bool coversMonitor=
        windowRect.left()<=full.left()+tolerance &&
        windowRect.top()<=full.top()+tolerance &&
        windowRect.right()-1>=full.right()-tolerance &&
        windowRect.bottom()-1>=full.bottom()-tolerance;
    // A maximized overlapped window can have invisible resize borders outside the
    // work area. Require a borderless foreground window as well, so normal maximized
    // browsers/editors never make Tony disappear.
    const bool isFullscreen=borderless && coversMonitor;
'''
count = text.count(old)
if count != 1:
    raise SystemExit(f'fullscreen detection anchor: expected 1 match, got {count}')
path.write_text(text.replace(old,new,1),encoding='utf-8')
print('TONY_FULLSCREEN_DETECTION_FIX=PASS')
