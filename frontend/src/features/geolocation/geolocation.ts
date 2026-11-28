import type { Device, Reading } from '../../lib/data.ts';
import { latestTimed, type Row } from '../../lib/studySf.ts';
// Mirrors accepted firmware study_sf_shore.h (8db4044). Not a legal boundary or coverage estimate.
export const SHORE = { latitude: 8.275292, longitude: 124.262623, enterM: 100, exitM: 150 } as const;
export const DEMO_SHORE = { latitude: 8.274, longitude: 124.2649 } as const;
export function shoreReference(synchronized: boolean) { return synchronized ? SHORE : DEMO_SHORE; }
export function distanceToShore(position: Pick<Reading, 'latitude' | 'longitude' | 'gpsFix'> | null | undefined, synchronized: boolean): number | null {
    if (!position?.gpsFix) return null;
    const { latitude: lat, longitude: lon } = position;
    if (lat === null || lon === null || !Number.isFinite(lat) || !Number.isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180) return null;
    const s = shoreReference(synchronized), rad = Math.PI / 180;
    const a = Math.sin((lat - s.latitude) * rad / 2) ** 2 + Math.cos(lat * rad) * Math.cos(s.latitude * rad) * Math.sin((lon - s.longitude) * rad / 2) ** 2;
    return 6371000 * 2 * Math.asin(Math.sqrt(Math.max(0, Math.min(1, a))));
}
export function distanceText(m: number | null): string { return m === null || !Number.isFinite(m) || m < 0 ? 'Unavailable' : m < 1000 ? `${Math.round(m)} m` : `${(m / 1000).toFixed(1)} km`; }
export type Pixel = { x: number; y: number };
// Calculate a genuine gap in screen pixels, so zoom never changes label readability.
export function distanceLayout(a: Pixel, b: Pixel, labelWidth: number) {
    const dx = b.x - a.x, dy = b.y - a.y, length = Math.hypot(dx, dy), center = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 };
    const gap = Math.hypot(labelWidth / 2, 9) + 5;
    if (length <= 2 * (gap + 20)) return { center: { x: center.x, y: center.y - 26 }, segments: [] as [Pixel, Pixel][] };
    const ratio = gap / length;
    return { center, segments: [[a, { x: center.x - dx * ratio, y: center.y - dy * ratio }], [{ x: center.x + dx * ratio, y: center.y + dy * ratio }, b]] as [Pixel, Pixel][] };
}
export type RegistryRow = { id: string; node_id: number; vessel_name: string | null; registration_serial_no: string | null };
export type RegistryState = 'ready' | 'unavailable' | 'not-configured';
export function registryDevices(devices: Device[], rows: RegistryRow[]): Device[] {
    const byNode = new Map<string, RegistryRow[]>();
    for (const r of rows) { if (!Number.isSafeInteger(r.node_id) || r.node_id <= 0 || typeof r.id !== 'string' || !r.id) continue; const k = String(r.node_id); byNode.set(k, [...(byNode.get(k) ?? []), r]); }
    const clean = (value: unknown) => typeof value === 'string' && value.trim() ? value.trim() : undefined;
    return devices.map(d => { const matches = byNode.get(d.id); if (matches?.length !== 1) return d; const r = matches[0]; return { ...d, registryId: r.id, registeredName: clean(r.vessel_name), registrationSerialNo: clean(r.registration_serial_no) }; });
}
// Receipt is independent of capture; latest timed GPS attempt is not the last valid fix.
export function vesselContext(records: Row[], id: string, now: number) {
    const rows = records.filter(r => String(r.recorder_node_id) === id);
    const timed = rows.filter(r => r.record_json.captured_at !== null && Number.isFinite(Date.parse(r.record_json.captured_at)) && Date.parse(r.record_json.captured_at) <= now);
    const attempt = latestTimed(timed.filter(r => r.record_json.kind === 'gps'));
    const capture = latestTimed(timed);
    const receipts = rows.map(r => Date.parse(r.synced_at)).filter(t => Number.isFinite(t) && t <= now);
    return { attemptAt: attempt ? Date.parse(attempt.record_json.captured_at!) : null, attemptFix: attempt ? attempt.record_json.payload.gps_fix === true : null, captureAt: capture ? Date.parse(capture.record_json.captured_at!) : null, receiptAt: receipts.length ? Math.max(...receipts) : null, unknownCaptureCount: rows.length - timed.length };
}
