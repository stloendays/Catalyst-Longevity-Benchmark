from pathlib import Path

path = Path("teddy_agent_pet/client/src/PetWindowV7.cpp")
text = path.read_text(encoding="utf-8")

old = '''    perchOnActiveWindow_=false;\n    QSettings().setValue("desktop/perch_on_active_window",false);\n    dockMode_=DockMode::Bottom;\n'''
new = '''    // Keep the user's active-window preference. Sleep blocks perching by itself;\n    // after Tony wakes, the preference can resume naturally.\n    dockMode_=DockMode::Bottom;\n'''
if text.count(old) != 1:
    raise RuntimeError(f"rest preference marker count={text.count(old)}")
text = text.replace(old, new, 1)

old = '''void PetWindow::mousePressEvent(QMouseEvent *e){\n    if(e->button()!=Qt::LeftButton) return;\n    mouseDown_=true;\n'''
new = '''void PetWindow::mousePressEvent(QMouseEvent *e){\n    if(e->button()!=Qt::LeftButton) return;\n    // Tony can be grabbed again while he is falling; user input always wins over physics.\n    if(falling_) {\n        falling_=false;\n        pendingDizzyAfterFall_=false;\n        physicsTimer_.stop();\n    }\n    mouseDown_=true;\n'''
if text.count(old) != 1:
    raise RuntimeError(f"mouse press marker count={text.count(old)}")
text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")
print("Tony V0.8.3 interaction polish applied")
