import { latestTimed, type Row } from '../../lib/studySf.ts';
import { eligibleRows, microCandidate, macroCandidate, observationTimes } from './wxbatPresentation.ts';

export type MicroObservation = { inputValid: boolean; state: string; pattern: string };
export type RecordedWeather = { captureAt: number | null; receiptAt: number | null; micro: MicroObservation | null };
const unavailable = (): RecordedWeather => ({ captureAt: null, receiptAt: null, micro: null });
function microObservation(value: unknown): MicroObservation | null {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
    const p = value as Record<string, unknown>;
    if (typeof p.input_valid !== 'boolean' || typeof p.state !== 'string' || typeof p.pattern !== 'string' || !p.state.trim() || !p.pattern.trim() || p.state.length > 64 || p.pattern.length > 64) return null;
    return { inputValid: p.input_valid, state: p.state, pattern: p.pattern };
}
// Select micro independently of macro-only records; retain legacy unavailable results.
export function recordedWeather(rows: Row[] | undefined, nodeId: string, now: number): RecordedWeather {
    if (!rows || !nodeId || !Number.isFinite(now)) return unavailable();
    const row = latestTimed(eligibleRows(rows, nodeId, now, 'weather').filter(r => microCandidate(r.record_json.payload)));
    if (!row) return unavailable();
    return { ...observationTimes(row, now), micro: macroCandidate(row.record_json.payload) ? null : microObservation(row.record_json.payload.micro) };
}
export function microStateLabel(micro: MicroObservation | null): string {
    if (!micro) return 'Unavailable';
    if (!micro.inputValid) return 'Invalid input';
    if (micro.state === 'WARMUP') return 'Warm-up';
    if (micro.state === 'INVALID') return 'Invalid state';
    if (micro.state === 'NO_PATTERN') return 'No pattern recorded';
    return micro.state;
}
// Only show a pattern as a result when the recorded inputs and state support it.
export function microPatternLabel(micro: MicroObservation | null): string {
    if (!micro || !micro.inputValid || micro.state === 'WARMUP' || micro.state === 'INVALID') return 'Unavailable';
    return micro.pattern === 'NONE' ? 'None recorded' : micro.pattern;
}
