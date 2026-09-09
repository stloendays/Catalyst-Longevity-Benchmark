from pathlib import Path

OLD_FULL = "催化剂寿命数据分析与实验辅助系统"
NEW_FULL = "催化剂长期稳定性评估与实验决策系统"
OLD_SHORT = "催化剂寿命分析系统"
NEW_SHORT = "催化剂稳定性决策系统"

allowed = {'.cpp', '.h', '.md', '.txt', '.yml', '.yaml', '.iss', '.cmake'}
changed = []
for path in Path('.').rglob('*'):
    if not path.is_file() or path.suffix.lower() not in allowed:
        continue
    if '.git' in path.parts or 'build' in path.parts:
        continue
    try:
        text = path.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        continue
    updated = text.replace(OLD_FULL, NEW_FULL).replace(OLD_SHORT, NEW_SHORT)
    if updated != text:
        path.write_text(updated, encoding='utf-8')
        changed.append(str(path))

print('\n'.join(changed))
if not changed:
    raise SystemExit('No product-name occurrences were updated')
