// Study observations, not live distress or live LoRa status.
export type Row = { recorder_node_id: number; record_id: string; record_json: { record_id: string; recorder_node_id: number; kind: 'weather' | 'gps' | 'sos' | 'cancel'; captured_at: string | null; uptime_ms: number; payload: Record<string, unknown> }; synced_at: string };
export const identity = (r: Row) => `${r.recorder_node_id}/${r.record_id}`;
export function projectStudy(rows: Row[]) {
    const unique = new Map<string, Row>();
    for (const r of rows) {
        if (r.record_id !== r.record_json.record_id || r.recorder_node_id !== r.record_json.recorder_node_id) throw Error('Record identity mismatch');
        const key = identity(r); if (unique.has(key)) throw Error('Unexpected duplicate database row'); unique.set(key, r);
    }
    const records = [...unique.values()];
    const events = new Map<string, { origin: number; event: number; sos: number; cancel: number; state: string }>();
    for (const r of records) {
        const { kind, payload: p } = r.record_json; if (kind !== 'sos' && kind !== 'cancel') continue;
        const origin = Number(p.origin_node_id), event = Number(p.event_id), key = `${origin}/${event}`;
        const e = events.get(key) ?? { origin, event, sos: 0, cancel: 0, state: '' }; e[kind]++; e.state = e.cancel ? 'Cancellation observed' : 'SOS observed; no cancellation recorded'; events.set(key, e);
    }
    return { records, weather: records.filter(r => r.record_json.kind === 'weather'), gps: records.filter(r => r.record_json.kind === 'gps'), events: [...events.values()], nodes: [...new Set(records.map(r => r.recorder_node_id))].sort((a, b) => a - b) };
}
// Never substitute upload time for capture time. Unknown time stays unknown.
export function latestTimed(rows: Row[]): Row | null {
    const timed = rows.filter(r => r.record_json.captured_at !== null && Number.isFinite(Date.parse(r.record_json.captured_at)));
    if (!timed.length) return null;
    const max = Math.max(...timed.map(r => Date.parse(r.record_json.captured_at!)));
    const ties = timed.filter(r => Date.parse(r.record_json.captured_at!) === max);
    // In one boot, counter order is known; across boots a tied timestamp is ambiguous.
    const boot = new Set(ties.map(r => r.record_id.split(':')[0]));
    if (boot.size !== 1) return null;
    return ties.sort((a, b) => Number(b.record_id.split(':')[1]) - Number(a.record_id.split(':')[1]))[0];
}
