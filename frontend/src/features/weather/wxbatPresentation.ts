import { latestTimed, type Row } from '../../lib/studySf.ts';
import { ageLabel, dateTime, weatherLabel } from '../../lib/data.ts';

export const batteryPercent = (x: unknown): number | null => typeof x === 'number' && Number.isInteger(x) && x >= 0 && x <= 100 ? x : null;
export type RecordedBattery = { value: number | null; captureAt: number | null; receiptAt: number | null };
export type MacroObservation = { inputValid: boolean; historyCount: number; state: number; index: number | null };
export type RecordedMacro = { macro: MacroObservation | null; captureAt: number | null; receiptAt: number | null };
const integer = (x: unknown, min: number, max: number): x is number => typeof x === 'number' && Number.isInteger(x) && x >= min && x <= max;
const object = (x: unknown): x is Record<string, unknown> => !!x && typeof x === 'object' && !Array.isArray(x);
export function eligibleRows(rows: Row[] | undefined, node: string, now: number, kind: Row['record_json']['kind']): Row[] {
    if (!rows || !node || !Number.isFinite(now)) return [];
    return rows.filter(r => String(r.recorder_node_id) === node && r.record_json.kind === kind && r.record_json.captured_at !== null && Number.isFinite(Date.parse(r.record_json.captured_at)) && Date.parse(r.record_json.captured_at) <= now);
}
export function observationTimes(row: Row | null, now: number) {
    if (!row) return { captureAt: null, receiptAt: null };
    const receipt = Date.parse(row.synced_at);
    return { captureAt: Date.parse(row.record_json.captured_at!), receiptAt: Number.isFinite(receipt) && receipt <= now ? receipt : null };
}
// Select the attempt, not the last valid position or last non-null battery.
export function recordedBattery(rows: Row[] | undefined, node: string, now: number): RecordedBattery {
    const row = latestTimed(eligibleRows(rows, node, now, 'gps'));
    return { value: batteryPercent(row?.record_json.payload.battery_percent), ...observationTimes(row, now) };
}
export function batteryLabel(sample: RecordedBattery | undefined): string {
    return sample?.value != null ? `${sample.value} %` : 'Unavailable';
}
export function batteryAge(sample: RecordedBattery | undefined, now: number): string {
    return sample?.captureAt != null ? `GPS observation ${ageLabel(sample.captureAt, now)}` : 'No unambiguous timed GPS observation';
}
export function batteryDetail(sample: RecordedBattery | undefined, now: number): string {
    return sample?.captureAt != null ? `${batteryLabel(sample)} · ${ageLabel(sample.captureAt, now)} (${dateTime(sample.captureAt)} PHT)` : 'Unavailable';
}
// A new macro-only record must not clear a micro result. Legacy records remain
// micro candidates, including legacy null (an explicit unavailable result).
export const macroCandidate = (p: Record<string, unknown>) => Object.hasOwn(p, 'macro') && p.macro !== null;
export const microCandidate = (p: Record<string, unknown>) => !Object.hasOwn(p, 'macro') || p.macro === null || p.micro !== null;
// Validate the display values defensively, without changing the wire contract.
export function macroObservation(p: Record<string, unknown>): MacroObservation | null {
    const m = p.macro;
    if (!object(m) || p.micro !== null || Object.keys(m).length !== 4 || !['input_valid', 'history_count', 'state', 'zambretti_index'].every(k => Object.hasOwn(m, k))) return null;
    const { input_valid: valid, history_count: count, state, zambretti_index: index } = m;
    if (typeof valid !== 'boolean' || !integer(count, 0, 10) || !integer(state, 0, 3) || state !== p.weather_state) return null;
    const trend = p.pressure_trend_hpa;
    if (trend !== null && !(typeof trend === 'number' && Number.isFinite(trend) && trend >= -200 && trend <= 200)) return null;
    const ready = valid && count === 10 && state !== 0 && typeof trend === 'number';
    if (ready ? !integer(index, 0, 255) : index !== null) return null;
    return { inputValid: valid, historyCount: count, state, index: index as number | null };
}
export function recordedMacro(rows: Row[] | undefined, node: string, now: number): RecordedMacro {
    const row = latestTimed(eligibleRows(rows, node, now, 'weather').filter(r => macroCandidate(r.record_json.payload)));
    return { macro: row ? macroObservation(row.record_json.payload) : null, ...observationTimes(row, now) };
}
export function macroResultLabel(m: MacroObservation | null): string {
    if (!m) return 'Unavailable';
    if (!m.inputValid) return 'Invalid input';
    if (m.historyCount < 10 || m.state === 0) return 'Calibrating';
    if (m.index === null) return 'Trend unavailable';
    return m.index >= 1 && m.index <= 32 ? 'Result recorded' : 'Unsupported index';
}
export function macroIndexLabel(m: MacroObservation | null): string {
    if (m?.index == null) return 'Unavailable';
    return `${m.index}${m.index < 1 || m.index > 32 ? ' (raw; unsupported)' : ''}`;
}
export const macroRecordedState = (m: MacroObservation | null) => m ? weatherLabel(m.state) : 'Unavailable';
