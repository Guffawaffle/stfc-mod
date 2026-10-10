"""Report verified Kir'Shara target removals and their captured call context.

Pass all rotated JSONL files from one session, e.g. community_kirshara_queue_*.jsonl.
Unknown snapshots are evidence gaps, never equivalent to an empty queue.
"""
import argparse
import collections
import json
from pathlib import Path


def analyze(events):
    spans = {e['span']: e for e in events if e.get('event') == 'span'}
    losses, gaps = [], []
    for event in sorted(spans.values(), key=lambda e: e['span']):
        before, after = event.get('before'), event.get('after')
        if not isinstance(before, list) or not isinstance(after, list):
            gaps.append({'span': event['span'], 'source': event['source'], 'reason': 'unknown_snapshot'})
            continue
        current = {q.get('fleet'): q for q in after}
        chain, parent, seen = [], event.get('parent', 0), set()
        while parent and parent not in seen:
            seen.add(parent)
            enclosing = spans.get(parent)
            if not enclosing:
                chain.append({'span': parent, 'source': 'missing_parent_capture'})
                break
            chain.append({'span': parent, 'source': enclosing['source'], 'notes': enclosing.get('notes', {})})
            parent = enclosing.get('parent', 0)
        for old in before:
            new = current.get(old.get('fleet'))
            if not old.get('valid') or not new or not new.get('valid'):
                gaps.append({'span': event['span'], 'fleet': old.get('fleet'), 'reason': 'queue_missing_or_unknown'})
                continue
            previous = collections.Counter(t['id'] for t in old['targets'])
            remaining = collections.Counter(t['id'] for t in new['targets'])
            removed = list((previous - remaining).elements())
            if removed:
                losses.append({'span': event['span'], 'wall_ms': event.get('wall_ms'),
                               'source': event['source'], 'fleet': old['fleet'], 'removed': removed,
                               'before_count': old['count'], 'after_count': new['count'],
                               'emptied': old['count'] > 0 and new['count'] == 0,
                               'context': chain, 'notes': event.get('notes', {}),
                               'recovery_enabled': event.get('recovery_enabled'),
                               'protection_enabled': event.get('protection_enabled'),
                               'stack': event.get('stack', {})})
    return {'sessions': [e for e in events if e.get('event') == 'session'],
            'spans': len(spans), 'removals': losses, 'unknown_snapshots': gaps,
            'capture_limited': any(e.get('event') == 'capture_limit' for e in events),
            'async_overruns': max((e.get('async_overruns', 0) for e in events), default=0),
            'interpretation': 'Nested spans can report the same removal; this identifies execution paths, not whether a removal was justified.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', nargs='+', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    events, malformed = [], []
    for path in args.logs:
        for line, raw in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
            if not raw.strip():
                continue
            try:
                event = json.loads(raw)
                if isinstance(event, dict) and event.get('schema') == 'kirshara-queue-science/v1':
                    events.append(event)
                else:
                    malformed.append(f'{path}:{line}: unexpected schema')
            except json.JSONDecodeError:
                malformed.append(f'{path}:{line}: malformed JSON')
    identities = {e.get('identity') for e in events if e.get('event') == 'session'}
    # Span IDs restart per process. Require a single capture prefix, including its .1/.2 rotations.
    prefixes = {p.name.split('.')[0] for p in args.logs}
    if len(prefixes) != 1 or len(identities) > 1:
        parser.error('Analyze one session and its rotations at a time.')
    report = analyze(events)
    report['malformed_lines'] = malformed
    content = json.dumps(report, indent=2) + '\n'
    if args.output:
        args.output.write_text(content, encoding='utf-8')
    else:
        print(content, end='')


if __name__ == '__main__':
    main()
