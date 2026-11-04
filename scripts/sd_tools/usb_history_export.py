#!/usr/bin/env python3
"""Read-only, checksummed USB export of F06 historical or legacy SD journals.

Commands are fixed/allowlisted by device firmware; no arbitrary path traversal.
This tool never writes to the SD card and refuses to overwrite local exports.
"""
from __future__ import annotations

import argparse
from datetime import datetime
from pathlib import Path
import re
import sys
import time

FILENAMES = ('sos_events.jsonl', 'telemetry.jsonl', 'diagnostics.jsonl')
SESSION_RE = re.compile(r'S[0-9A-F]{7}\Z')
MAX_FILE_BYTES = 1048576


def fnv1a(data: bytes) -> int:
    value = 2166136261
    for b in data:
        value = ((value ^ b) * 16777619) & 0xffffffff
    return value


def consume_export_line(line: str, state: dict, expected_target: str) -> bool:
    """Pure protocol validator; returns True on SESSION_END."""
    if not line.startswith('[SDH] '):
        return False
    parts = line.split(' ', 4)
    if len(parts) < 2:
        return False
    kind = parts[1]
    if kind == 'ERROR':
        raise ValueError(f'Device refused export: {line}')
    if kind == 'SESSION_START':
        if state['started'] or line != f'[SDH] SESSION_START target={expected_target} files=3':
            raise ValueError('Wrong target or repeated SESSION_START')
        state['started'] = True
        return False
    if not state['started']:
        # Keep waiting for the matching session; serial boot logs are ignored.
        return False
    if kind == 'BEGIN':
        if len(parts) != 5:
            raise ValueError('Malformed BEGIN')
        idx, name, size = int(parts[2]), parts[3], int(parts[4])
        if idx not in range(3) or name != FILENAMES[idx] or idx != len(state['buffers']):
            raise ValueError('Unexpected file name or file index')
        if size < 0 or size > MAX_FILE_BYTES:
            raise ValueError('Invalid declared size')
        state['buffers'][idx] = bytearray()
        state['sizes'][idx] = size
    elif kind == 'DATA':
        if len(parts) != 5:
            raise ValueError('Malformed DATA')
        idx, offset = int(parts[2]), int(parts[3])
        raw_hex = parts[4]
        if idx not in state['buffers'] or idx in state['complete']:
            raise ValueError('Unexpected DATA index')
        if not raw_hex or len(raw_hex) > 128 or len(raw_hex) % 2 or re.fullmatch(r'[0-9A-F]+', raw_hex) is None:
            raise ValueError('Invalid DATA hex')
        b = state['buffers'][idx]
        if offset != len(b):
            raise ValueError('Missing/out-of-order DATA chunk')
        chunk = bytes.fromhex(raw_hex)
        if len(b) + len(chunk) > state['sizes'][idx]:
            raise ValueError('DATA exceeded declared size')
        b.extend(chunk)
    elif kind == 'END':
        if len(parts) != 5:
            raise ValueError('Malformed END')
        idx, size, digest = int(parts[2]), int(parts[3]), int(parts[4], 16)
        if (idx not in state['buffers'] or idx in state['complete'] or
            len(state['buffers'][idx]) != size or size != state['sizes'][idx] or
            fnv1a(state['buffers'][idx]) != digest):
            raise ValueError('SD source length or checksum mismatch')
        state['complete'].add(idx)
        state['checksums'][idx] = f'{digest:08X}'
        print(f'CHECKED {FILENAMES[idx]}: {size} bytes, FNV1A32={digest:08X}', flush=True)
    elif kind == 'SESSION_END':
        if not state['started'] or state['complete'] != {0, 1, 2}:
            raise ValueError('Incomplete history session')
        return True
    else:
        raise ValueError(f'Unexpected SDH protocol record: {line}')
    return False


def new_state() -> dict:
    return dict(started=False, buffers={}, sizes={}, checksums={}, complete=set())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    choice = ap.add_mutually_exclusive_group(required=True)
    choice.add_argument('--list', action='store_true', help='List F06 sessions on Vessel-002')
    choice.add_argument('--legacy', action='store_true', help='Export original Vessel-002 journals as read-only files')
    choice.add_argument('--session', help='Export earlier F06 session (for example S41F5724)')
    ap.add_argument('--port', default='COM13')
    ap.add_argument('--out', type=Path, default=None, help='New local directory; must not exist')
    ap.add_argument('--timeout', type=int, default=600)
    args = ap.parse_args()
    if args.session and SESSION_RE.fullmatch(args.session) is None:
        ap.error('--session must exactly match S and 7 uppercase hexadecimal digits')
    if args.timeout < 30 or args.timeout > 1800:
        ap.error('--timeout must be between 30 and 1800 seconds')
    target = 'LEGACY' if args.legacy else args.session
    out = args.out or Path('logs') / (
        f'f06_usb_history_{target}_{datetime.now().strftime("%Y%m%d_%H%M%S")}'
    )
    if not args.list and out.exists():
        raise SystemExit(f'REFUSE overwrite: output directory already exists: {out}')
    try:
        import serial
    except ImportError:
        raise SystemExit('pyserial is required: install into your PlatformIO Python environment')
    command = 'SD LIST\n' if args.list else f'SD READ {target}\n'
    with serial.Serial(args.port, 115200, timeout=.5, write_timeout=3) as port:
        # Some boards reset when COM is opened. The requested historical target
        # is stable across boot even when a new active sandbox is created.
        time.sleep(5)
        port.reset_input_buffer()
        port.write(command.encode('ascii'))
        port.flush()
        print(f'Requested {command.strip()} over {args.port} (SD read-only)', flush=True)
        deadline = time.monotonic() + args.timeout
        state = new_state()
        seen_list = False
        finished = False
        while time.monotonic() < deadline:
            raw = port.readline()
            line = raw.decode('utf-8', errors='replace').strip()
            if not line:
                continue
            if not line.startswith('[SDH] '):
                continue
            if args.list:
                if line.startswith('[SDH] ERROR '):
                    raise ValueError(line)
                if line == '[SDH] LIST_BEGIN scope=V002 sessions_only=1':
                    seen_list = True
                if seen_list:
                    print(line, flush=True)
                if seen_list and line.startswith('[SDH] LIST_END '):
                    finished = True
                    break
            else:
                if consume_export_line(line, state, target):
                    finished = True
                    break
        if not finished:
            raise ValueError('Timeout or incomplete SDH response; no local files saved')
    if args.list:
        print('LIST COMPLETE. No SD or local journal changes.', flush=True)
        return 0
    if state['complete'] != {0, 1, 2}:
        raise ValueError('Incomplete export: refusing local save')
    out.mkdir(parents=True, exist_ok=False)
    import hashlib
    import json
    manifest = dict(target=target, port=args.port, files=[])
    for i, filename in enumerate(FILENAMES):
        data = bytes(state['buffers'][i])
        dest = out / filename
        with dest.open('xb') as handle:
            handle.write(data)
        manifest['files'].append(dict(name=filename, bytes=len(data),
            sha256=hashlib.sha256(data).hexdigest().upper(), fnv1a32=state['checksums'][i]))
        print(f'SAVED {dest}: {len(data)} bytes SHA256={manifest["files"][-1]["sha256"]}', flush=True)
    (out / 'sha256_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'VERIFIED HISTORICAL EXPORT COMPLETE: {out}', flush=True)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError) as exc:
        print(f'STOP: {exc}', file=sys.stderr)
        sys.exit(1)
