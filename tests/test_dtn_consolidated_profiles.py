from pathlib import Path
import configparser, re, shlex
root=Path(__file__).resolve().parents[1]
active=(root/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
history=(root/'tests/fixtures/dtn/sf7_intermediate_profiles.ini').read_text(encoding='utf-8')
def parse(s):
    c=configparser.ConfigParser(interpolation=None);c.read_string(s);return c
a=parse(active); h=parse(active+'\n'+history)
def flags(c,n):
    return shlex.split(re.sub(r'\$\{([^}]+)\.build_flags\}',lambda m:' '.join(flags(c,m[1])),c[n]['build_flags']))
base='env:tbeam-supreme-study-sf-build-only'; prior='env:tbeam-supreme-f26c-maintenance-deny-build-only'
expected=flags(h,prior)+['-D','THESIS_STUDY_SF_BUILD_ONLY=1','-Wl,--undefined=thesis_study_sf_link_anchor']
assert flags(a,base)==expected
for n,flag in [('tbeam-supreme-study-sf-v003-offline-bench','THESIS_STUDY_SF_OFFLINE_V003=1'),('tbeam-supreme-study-sf-v003-sync-bench','THESIS_STUDY_SF_SYNC_V003=1'),('tbeam-supreme-universal-study','THESIS_STUDY_SF_UNIVERSAL_OFFLINE=1')]:
    assert flags(a,'env:'+n)==expected+['-D',flag]
assert len(a.sections())==55 and len(h.sections())==84
assert not any('multihop-' in n or re.match(r'env:tbeam-supreme-f2[456]',n) for n in a.sections())
print('DTN consolidated profile equivalence PASS (ordered flags; not compiler or physical evidence)')
