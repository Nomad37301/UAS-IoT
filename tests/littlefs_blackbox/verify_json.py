import json
from pathlib import Path

records = [json.loads(line) for line in Path('.build/blackbox-json-samples.jsonl').read_text().splitlines()]
assert len(records) == 2
assert records[0]['system'] == 'AETHER'
assert records[0]['session'] == '00001234'
assert records[0]['seq'] == 0 and records[0]['uptime_ms'] == 42
assert records[0]['message'] == 'quote " path \\\n\t'
assert records[1]['seq'] == 1 and records[1]['uptime_ms'] == 4294967295
assert records[1]['message'] == '\x01' * 96
print('PASS: independent JSON parser, quotes/backslash/control characters, numeric fields')
