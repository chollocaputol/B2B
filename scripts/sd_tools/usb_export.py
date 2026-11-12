import argparse
import pathlib
import time
import serial

p = argparse.ArgumentParser(description='Read-only THESIS JSONL USB export')
p.add_argument('--port', default='COM13')
p.add_argument('--out', default='logs/phase2c_sd_readback')
p.add_argument('--timeout', type=int, default=600)
a = p.parse_args()
out = pathlib.Path(a.out)
out.mkdir(parents=True, exist_ok=True)
files = ['sos_events.jsonl', 'telemetry.jsonl', 'diagnostics.jsonl']
buffers = {}
lengths = {}
checksums = {}
completed = set()
started = False
end_seen = False

def fnv(data):
    value = 2166136261
    for b in data:
        value = ((value ^ b) * 16777619) & 0xFFFFFFFF
    return value

with serial.Serial(a.port, 115200, timeout=.4, write_timeout=3) as port:
    time.sleep(2)
    port.reset_input_buffer()
    port.write(b'SD EXPORT\n')
    port.flush()
    print('Export requested; waiting for framed records...', flush=True)
    deadline = time.monotonic() + a.timeout
    while time.monotonic() < deadline:
        line = port.readline().decode('utf-8', errors='replace').strip()
        if not line or not line.startswith('[SDX] '):
            continue
        print(line[:180], flush=True) if line.startswith(('[SDX] BEGIN', '[SDX] END', '[SDX] ERROR', '[SDX] SESSION')) else None
        parts = line.split(' ', 4)
        kind = parts[1]
        if kind == 'ERROR':
            raise SystemExit('Device reported export error; no source files modified.')
        if kind == 'SESSION_START':
            started = True
        elif kind == 'BEGIN':
            idx = int(parts[2]); name = parts[3]; size = int(parts[4])
            if idx not in range(3) or name != files[idx] or idx in buffers:
                raise SystemExit('Unexpected BEGIN header')
            buffers[idx] = bytearray(); lengths[idx] = size
        elif kind == 'DATA':
            idx = int(parts[2]); offset = int(parts[3]); hex_text = parts[4]
            if idx not in buffers or idx in completed or len(buffers[idx]) != offset:
                raise SystemExit('Missing/out-of-order DATA chunk')
            buffers[idx].extend(bytes.fromhex(hex_text))
        elif kind == 'END':
            idx = int(parts[2]); size = int(parts[3]); digest = int(parts[4], 16)
            if idx not in buffers or len(buffers[idx]) != lengths[idx] or size != lengths[idx] or fnv(buffers[idx]) != digest:
                raise SystemExit('Size or checksum mismatch. STOP')
            completed.add(idx)
            checksums[idx] = digest
        elif kind == 'SESSION_END':
            end_seen = True
            break
if not (started and end_seen and completed == {0,1,2}):
    raise SystemExit(f'Incomplete export, completed files: {sorted(completed)}. Files not saved.')
for i, name in enumerate(files):
    destination = out / name
    if destination.exists():
        raise SystemExit(f'Output already exists: {destination}. STOP; do not overwrite.')
for i, name in enumerate(files):
    (out / name).write_bytes(buffers[i])
    print(f'VERIFIED {name}: {len(buffers[i])} bytes FNV1A32={checksums[i]:08X}')
print('EXPORT COMPLETE - all three files verified and saved locally.')
