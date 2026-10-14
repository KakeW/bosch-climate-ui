#!/usr/bin/env python3
"""Replace only the single local Midea source. Never rewrite secrets or entities."""
import argparse, re, shutil, datetime
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('config',type=Path);p.add_argument('--apply',action='store_true');p.add_argument('--ref',default='main');a=p.parse_args()
if not re.fullmatch(r'[A-Za-z0-9_.\-/]+',a.ref):p.error('Invalid Git ref')
s=a.config.read_text();pattern=r'(?m)^external_components:\n  - source:\n      type: local\n      path: components\n    components: \[midea\]\n'
replacement=f'external_components:\n  - source: github://KakeW/bosch-climate-ui@{a.ref}\n    components: [midea]\n    refresh: 1h\n'
if replacement in s:print('Already migrated; no changes.');raise SystemExit(0)
if len(re.findall(pattern,s))!=1:p.error('Expected exactly one local Midea source block; no file changed.')
new=re.sub(pattern,lambda _:replacement,s,count=1)
if not a.apply:print('Preview: only the local Midea source will change to GitHub. Use --apply to save.');raise SystemExit(0)
backup=a.config.with_name(a.config.name+'.backup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'));shutil.copy2(a.config,backup);a.config.write_text(new)
print('Midea source migrated. All other YAML text preserved. Backup:',backup)
