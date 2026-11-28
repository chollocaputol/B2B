import type { Device, Reading, SosEvent } from '../../lib/data.ts';
import { describeRecord, orderStudyHistory } from '../../lib/studyDashboardData.ts';
import { emergencyReports, reportTime } from '../geolocation/vesselPopup.ts';
import type { Row } from '../../lib/studySf.ts';
import { indexViews, matchedProjection, matchedEvent, timeValue, NO_VIEWS, type ViewSnapshot } from '../../lib/relationalViews.ts';
export type Kind = Row['record_json']['kind'];
export type LogEntry = { key: string; node: string; vessel: string; registration: string; kind: Kind; capture: number | null; receipt: number | null; description: string; source: string; raw?: Row; projected: boolean };
const phTime = new Intl.DateTimeFormat('en-PH', { timeZone: 'Asia/Manila', year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });
export const fullTime = (t: number | null) => t === null ? 'Unknown' : phTime.format(t);
const day = (t: number | null) => t === null ? '' : new Date(t + 8 * 3600000).toISOString().slice(0, 10);
const label = (devices: Device[], id: string) => { const d = devices.find(v => v.id === id); return d?.registeredName ? `${d.name} · ${d.registeredName}` : d?.name ?? `Vessel ${id}`; };
const value = (n: number | null, u: string) => n === null || !Number.isFinite(n) ? 'Unavailable' : `${n} ${u}`;
function observation(r: Row) { const p = r.record_json.payload; return r.record_json.kind === 'sos' || r.record_json.kind === 'cancel' ? `${r.record_json.kind.toUpperCase()} · origin ${p.origin_node_id ?? 'Unknown'}, event ${p.event_id ?? 'Unknown'}` : describeRecord(r); }
export function hostedLogs(rows: Row[], devices: Device[], views: ViewSnapshot = NO_VIEWS): LogEntry[] {
    const index = indexViews(views);
    return orderStudyHistory(rows).map(raw => {
        const projected = matchedProjection(raw, index), r = projected ?? raw, node = String(r.recorder_node_id), p = r.record_json.payload, kind = r.record_json.kind;
        return { key: `${r.recorder_node_id}/${r.record_id}`, node, vessel: label(devices, node), registration: devices.find(d => d.id === node)?.registrationSerialNo ?? '', kind, capture: timeValue(r.record_json.captured_at), receipt: timeValue(r.synced_at), description: observation(r), source: kind === 'sos' || kind === 'cancel' ? (p.source === 'local' ? 'Local' : p.source === 'received' ? 'Received' : 'Unknown') : 'Device observation', raw, projected: !!projected };
    });
}
export function demoLogs(readings: Reading[], samples: SosEvent[], devices: Device[]): LogEntry[] {
    const rows: LogEntry[] = [];
    function add(node: string, capture: number, kind: Kind, description: string) { if (!Number.isFinite(capture)) return; rows.push({ key: `sample/${node}/${kind}/${capture}`, node, vessel: label(devices, node), registration: '', capture, receipt: null, kind, description, source: 'Sample', projected: false }); }
    for (const r of readings) { add(r.deviceId, r.timestamp, 'weather', `${value(r.temperature, '°C')}; ${value(r.humidity, '%')}; ${value(r.pressure, 'hPa')}; trend ${value(r.pressureTrend, 'hPa')}`); add(r.deviceId, r.timestamp, 'gps', r.gpsFix && r.latitude !== null && r.longitude !== null ? `GPS fix: ${r.latitude}, ${r.longitude}` : 'No GPS fix'); }
    for (const r of samples) add(r.deviceId, r.timestamp, 'sos', `Sample SOS · origin ${label(devices, r.deviceId)} · event coordinates ${r.latitude}, ${r.longitude}`);
    return rows.sort((a, b) => b.capture! - a.capture! || a.key.localeCompare(b.key));
}
export type LogFilters = { query: string; node: string; kind: string; from: string; to: string };
export const EMPTY_FILTERS: LogFilters = { query: '', node: '', kind: '', from: '', to: '' };
export function filterLogs(rows: LogEntry[], f: LogFilters) {
    const terms = f.query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
    return rows.filter(r => {
        if (f.node && r.node !== f.node || f.kind && r.kind !== f.kind) return false;
        const d = day(r.capture); if ((f.from || f.to) && (!d || (f.from && d < f.from) || (f.to && d > f.to))) return false;
        const search = [r.key, r.node, r.vessel, r.registration, r.kind, r.description, r.source, fullTime(r.capture), d, fullTime(r.receipt), day(r.receipt), r.raw?.record_json.captured_at, r.raw?.synced_at].join(' ').toLocaleLowerCase();
        return terms.every(t => search.includes(t));
    });
}
export function groupedSos(now: number, records: Row[] | undefined, samples: SosEvent[], devices: Device[], views: ViewSnapshot = NO_VIEWS) {
    const index = indexViews(views);
    return emergencyReports(now, records, samples).map(report => {
        const rows = (records ?? []).filter(r => ['sos', 'cancel'].includes(r.record_json.kind) && String(r.record_json.payload.origin_node_id) === report.originId && r.record_json.payload.event_id === report.eventId);
        const matched = matchedEvent(index, report.originId, report.eventId, rows, report.sosCount, report.cancelCount);
        const receipts = rows.map(r => timeValue(r.synced_at)).filter((t): t is number => t !== null && t <= now);
        const rawReceipt = receipts.length ? Math.max(...receipts) : null;
        return { ...report, vessel: label(devices, report.originId), lastReceipt: matched && matched.lastReceipt === rawReceipt ? matched.lastReceipt : rawReceipt, recordingNodes: matched?.recordingNodes ?? new Set(rows.map(r => r.recorder_node_id)).size, projected: !!matched };
    }).sort((a, b) => Number(b.status === 'Needs attention') - Number(a.status === 'Needs attention') || (reportTime(b) ?? -Infinity) - (reportTime(a) ?? -Infinity) || a.key.localeCompare(b.key));
}
