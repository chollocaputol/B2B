#!/usr/bin/env python3
"""Enroll a new identical-hardware vessel by USB with unique ID in its NVS.

This CLI intentionally DOES NOT flash, erase, reset, modify SD, or touch other
NVS namespaces. Build the reviewed universal firmware separately first.
Preview is read-only; --apply requires a local operator confirmation.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import time

RE_STATUS=re.compile(r'^\[FLEET\] STATUS MAC=([0-9A-F]{12}) STATE=(READY|BLANK|INVALID) ID=(\d+) LABEL=(\S+)$')
RE_ENROLLED=re.compile(r'^\[FLEET\] ENROLLED MAC=([0-9A-F]{12}) ID=(\d+) LABEL=(\S+) REBOOT_REQUIRED=1$')
DEFAULT_RESERVED={1,2,3}

def fail(message):
    raise RuntimeError('STOP: '+str(message))

def label(vessel_id):
    if not 1<=vessel_id<=9999: fail('vessel ID must be 1..9999')
    return f'Vessel-{vessel_id:03d}'

def parse_status(line):
    m=RE_STATUS.fullmatch(line.strip())
    if not m: return None
    mac,state,raw_id,lbl=m.groups()
    vessel_id=int(raw_id)
    if state=='READY' and (not 1<=vessel_id<=9999 or label(vessel_id)!=lbl):
        fail('firmware status inconsistent with vessel label')
    if state!='READY' and (vessel_id!=0 or lbl!='UNPROVISION'):
        fail('invalid unprovisioned status')
    return {'mac':mac,'state':state,'id':vessel_id,'label':lbl}

def clean_registry(document):
    if not isinstance(document,dict) or document.get('schema')!=1: fail('unknown registry schema')
    devices=document.get('devices')
    if not isinstance(devices,dict): fail('invalid registry devices')
    used={}
    for mac,entry in devices.items():
        if not re.fullmatch('[0-9A-F]{12}',mac): fail('invalid registry MAC')
        if not isinstance(entry,dict) or not isinstance(entry.get('id'),int): fail('invalid registry entry')
        vid=entry['id']
        if not 1<=vid<=9999 or entry.get('label')!=label(vid): fail('invalid registry vessel label')
        if vid in used and used[vid]!=mac: fail('DUPLICATE VESSEL ID '+str(vid))
        used[vid]=mac
    return document

def load_registry(path):
    if not path.exists(): return {'schema':1,'devices':{}}
    return clean_registry(json.loads(path.read_text(encoding='utf-8')))

def plan(status, registry):
    existing=registry['devices'].get(status['mac'])
    all_ids={entry['id']:mac for mac,entry in registry['devices'].items()}
    if status['state']=='INVALID': fail('existing invalid NVS identity; manual recovery required')
    if status['state']=='READY':
        if existing and existing['id']!=status['id']: fail('this MAC is mapped to a different vessel ID')
        owner=all_ids.get(status['id'])
        if owner is not None and owner!=status['mac']: fail('DUPLICATE VESSEL ID')
        return {'action':'register-existing' if existing is None else 'already-registered','id':status['id']}
    if existing: fail('device previously registered but now has blank NVS; investigate before writing')
    used=set(all_ids)|DEFAULT_RESERVED
    new_id=next((i for i in range(4,10000) if i not in used),None)
    if new_id is None: fail('registry exhausted')
    return {'action':'enroll-new','id':new_id}

def get_status(serial_port, timeout=8):
    serial_port.reset_input_buffer()
    serial_port.write(b'FLEET STATUS\n')
    serial_port.flush()
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        line=serial_port.readline().decode('utf-8','replace').strip()
        response=parse_status(line)
        if response: return response
        if '[FLEET] ERROR' in line: fail('device refused STATUS: '+line)
    fail('No FLEET STATUS reply; install a firmware with USB enrollment support')

def wait_enrolled(port,mac,expected_id,timeout=8):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        line=port.readline().decode('utf-8','replace').strip()
        if '[FLEET] ERROR' in line: fail('device rejected enrollment: '+line)
        m=RE_ENROLLED.fullmatch(line)
        if m:
            got_mac,got_id,got_label=m.groups()
            if got_mac!=mac or int(got_id)!=expected_id or got_label!=label(expected_id):
                fail('device enrollment acknowledgement mismatch')
            return
    fail('Missing enrollment acknowledgement; requery device before retry')

def persist_registry(path,registry):
    path.parent.mkdir(parents=True,exist_ok=True)
    # Never replace a registry using an unreviewed backup or unsafe partial JSON.
    clean_registry(registry)
    encoded=(json.dumps(registry,indent=2,sort_keys=True)+'\n').encode('utf-8')
    if path.exists():
        backup=path.with_name(path.name+'.backup.'+time.strftime('%Y%m%d_%H%M%S'))
        if backup.exists(): fail('registry backup collision; rerun after checking clock')
        backup.write_bytes(path.read_bytes())
    fd,name=tempfile.mkstemp(prefix='.registry-',suffix='.tmp',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as stream:
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name,path)
    finally:
        if os.path.exists(name): os.unlink(name)

def run(args):
    import serial  # pyserial is part of the PlatformIO Python environment
    path=Path(args.registry).expanduser().resolve()
    if path.is_relative_to(Path(args.repo).resolve()):
        fail('registry MUST live outside the Git repository')
    lock=path.with_name(path.name+'.lock')
    if lock.exists(): fail('registry lock already exists; investigate concurrent/stale enrollment')
    # Lock even previews to prevent concurrent allocation against a stale file.
    lock.parent.mkdir(parents=True,exist_ok=True)
    try:
        fd=os.open(lock,os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
    except FileExistsError:
        fail('another enrollment process holds the registry lock')
    try:
        with os.fdopen(fd,'w') as stream: stream.write(str(os.getpid())+'\n')
        registry=load_registry(path)
        with serial.Serial(args.port,115200,timeout=0.25,write_timeout=2) as device:
            time.sleep(1.5)  # allow USB CDC to reconnect after serial opening
            status=get_status(device)
            result=plan(status,registry)
            print('DEVICE',status['mac'],'NVS',status['state'],'ID',status['id'])
            print('PLAN',result['action'],label(result['id']))
            if not args.apply:
                print('READ-ONLY PREVIEW. To enroll/register, rerun with --apply. NO FLASH.')
                return
            if result['action']=='already-registered':
                print('PASS: Existing identity and registry agree; NO NVS WRITES.')
                return
            exact=f'ENROLL {label(result["id"])}'
            if input(f'Type {exact} to authorize: ').strip()!=exact:
                fail('operator authorization not provided; NO NVS WRITES')
            if result['action']=='enroll-new':
                # Double-query the live device to detect a reset/identity change.
                if get_status(device)!=status: fail('device changed before enrollment')
                device.write(f'FLEET ENROLL {result["id"]} {status["mac"]}\n'.encode('ascii'))
                device.flush()
                wait_enrolled(device,status['mac'],result['id'])
                verified=get_status(device)
                if verified['state']!='READY' or verified['id']!=result['id'] or verified['mac']!=status['mac']:
                    fail('NVS verification failed; registry NOT updated')
            else:
                verified=get_status(device)
                if verified!=status: fail('existing vessel state changed')
            registry['devices'][status['mac']]={'id':result['id'],'label':label(result['id'])}
            persist_registry(path,registry)
            print('PASS:',label(result['id']),'registered; registry:',path)
            if result['action']=='enroll-new':
                print('REBOOT DEVICE before RF/SD use; this boot initialized with a blank identity.')
    finally:
        lock.unlink(missing_ok=True)

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',help='USB serial COM port, e.g. COM5')
    p.add_argument('--registry',default=str(Path.home()/'Documents'/'THESIS'/'fleet_registry.json'))
    p.add_argument('--repo',default=str(Path(__file__).resolve().parents[2]))
    p.add_argument('--apply',action='store_true',help='explicit operator-confirmed one-time enrollment')
    p.add_argument('--self-test',action='store_true')
    args=p.parse_args(argv)
    if args.self_test:
        tests()
        return 0
    if not args.port: p.error('--port is required unless --self-test')
    try: run(args)
    except (RuntimeError,OSError,ValueError,ImportError) as e:
        print('STOP:',e,file=sys.stderr)
        return 1
    return 0

def tests():
    assert parse_status('[FLEET] STATUS MAC=ABCDEF123456 STATE=BLANK ID=0 LABEL=UNPROVISION')['state']=='BLANK'
    r={'schema':1,'devices':{'AABBCCDDEEFF':{'id':1,'label':'Vessel-001'}}}
    assert plan({'mac':'ABCDEF123456','state':'BLANK','id':0,'label':'UNPROVISION'},r)['id']==4
    assert plan({'mac':'AABBCCDDEEFF','state':'READY','id':1,'label':'Vessel-001'},r)['action']=='already-registered'
    assert label(9999)=='Vessel-9999'
    for bad in [{'schema':2,'devices':{}}, {'schema':1,'devices':{'AABBCCDDEEFF':{'id':0,'label':'Vessel-000'}}},
                {'schema':1,'devices':{'AABBCCDDEEFF':{'id':4,'label':'Vessel-004'},'112233445566':{'id':4,'label':'Vessel-004'}}}]:
        try: clean_registry(bad)
        except RuntimeError: pass
        else: raise AssertionError('unsafe registry accepted')
    print('PASS: identity parsing, reserved IDs, collision rejection, registry validation')

if __name__=='__main__': sys.exit(main())
