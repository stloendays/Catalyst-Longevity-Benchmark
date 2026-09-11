#!/usr/bin/env python3
"""Compile Tony's English-only Paula-boyfriend corpus into deterministic SFT splits."""

from __future__ import annotations

import hashlib
import json
from collections import Counter
from pathlib import Path

from boyfriend_expansion_v3 import build_examples

ROOT = Path('bear_agent_pet/training')
SOURCE = ROOT / 'source/tony_boyfriend_en_v3.json'
OUTPUT = ROOT / 'processed/tony_boyfriend_en_v3'


def bucket(text: str, n: int = 8) -> int:
    return int.from_bytes(hashlib.sha256(text.encode('utf-8')).digest()[:4], 'big') % n


def main() -> None:
    data = json.loads(SOURCE.read_text(encoding='utf-8'))
    system = str(data['system']).strip()
    examples = list(data['examples']) + build_examples()
    OUTPUT.mkdir(parents=True, exist_ok=True)

    seen: set[str] = set()
    rows: list[dict] = []
    curated_count = len(data['examples'])
    for index, ex in enumerate(examples):
        user = str(ex['user']).strip()
        assistant = str(ex['assistant']).strip()
        key = user.casefold()
        if not user or not assistant or key in seen:
            continue
        seen.add(key)
        if any('\u4e00' <= ch <= '\u9fff' for ch in user + assistant):
            raise ValueError(f'Non-English CJK text in row {index}: {user!r}')
        rows.append({
            'messages': [
                {'role': 'system', 'content': system},
                {'role': 'user', 'content': user},
                {'role': 'assistant', 'content': assistant},
            ],
            'tags': list(ex.get('tags') or []),
            'action': str(ex.get('action') or 'idle'),
            'emotion': str(ex.get('emotion') or 'warm'),
            'source': 'tony_boyfriend_en_v3_curated' if index < curated_count else 'tony_boyfriend_en_v3_expansion',
            'weight': float(ex.get('weight', 1.0 if index < curated_count else 0.82)),
        })

    train, eval_ = [], []
    for row in rows:
        (eval_ if bucket(row['messages'][1]['content']) == 0 else train).append(row)

    def write(name: str, values: list[dict]) -> None:
        with (OUTPUT / name).open('w', encoding='utf-8', newline='\n') as f:
            for row in values:
                f.write(json.dumps(row, ensure_ascii=False) + '\n')

    write('all.jsonl', rows)
    write('train.jsonl', train)
    write('eval.jsonl', eval_)

    manifest = {
        'name': 'Tony English Paula Boyfriend Corpus',
        'version': '3.0',
        'language': 'en',
        'role': 'Paula-boyfriend',
        'total': len(rows),
        'train': len(train),
        'eval': len(eval_),
        'curated': sum(r['source'].endswith('_curated') for r in rows),
        'expanded': sum(r['source'].endswith('_expansion') for r in rows),
        'split': 'deterministic_sha256_bucket_1_of_8_eval',
        'action_counts': dict(sorted(Counter(r['action'] for r in rows).items())),
        'tag_counts': dict(sorted(Counter(t for r in rows for t in r['tags']).items())),
        'notes': 'English-only social companion corpus. No chemistry, coding, server, OpenClaw, or tool-use training examples.',
    }
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
