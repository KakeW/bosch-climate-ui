import tempfile,subprocess,sys
from pathlib import Path
script=Path(__file__).resolve().parents[1]/'scripts/migrate-esphome.py'
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'device.yaml'
 before='climate:\n  - platform: midea\n    id: existing\n    name: Existing\nexternal_components:\n  - source:\n      type: local\n      path: components\n    components: [midea]\napi:\n  encryption:\n    key: !secret api_key\n'
 p.write_text(before)
 subprocess.run([sys.executable,str(script),str(p)],check=True,capture_output=True);assert p.read_text()==before
 subprocess.run([sys.executable,str(script),str(p),'--apply'],check=True,capture_output=True)
 after=p.read_text();assert after.split('external_components:')[0]==before.split('external_components:')[0];assert after[after.index('api:'):]==before[before.index('api:'):];assert list(Path(d).glob('*.backup-*'))[0].read_text()==before
 subprocess.run([sys.executable,str(script),str(p),'--apply'],check=True,capture_output=True);assert p.read_text()==after
 p.write_text('external_components: []\n');r=subprocess.run([sys.executable,str(script),str(p),'--apply'],capture_output=True);assert r.returncode!=0 and p.read_text()=='external_components: []\n'
print('PASS: dry run, exact preservation, backup, idempotency, refusal of ambiguous layout')
