import { useEffect, useRef, useState } from 'react';
import type { MapSession } from '../geolocation/VesselMap';
import App from '../../App';
import { getSupabaseClient } from '../../lib/supabase';
import type { Row } from '../../lib/studySf';
import { excludeKnownFixtures } from '../../lib/studyDashboardData';
import { studyAccessMode } from './studyAccessMode';
import { snapshotPhase, isAccessFailure } from '../../lib/refreshState';
import { operatorDestination } from '../../routes';
import type { RegistryRow, RegistryState } from '../geolocation/geolocation';
import { readRelationalViews, ViewAccessError, NO_VIEWS, type ViewSnapshot } from '../../lib/relationalViews';
const EMPTY_ROWS: Row[] = [];

function route(path: string) {
    if (location.pathname !== path || location.search || location.hash) history.replaceState(null, '', path);
}
export default function StudyAccess() {
    const mapSession = useRef<MapSession>({});
    const requested = useRef(operatorDestination(location.pathname, location.hash));
    const [starting, setStarting] = useState(true);
    const [slowStart, setSlowStart] = useState(false);
    useEffect(() => {
        if (!starting) return;
        const timer = window.setTimeout(() => setSlowStart(true), 500);
        return () => window.clearTimeout(timer);
    }, [starting]);
    const started = useRef(false);
    const inFlight = useRef(false);
    const [logoutFailed, setLogoutFailed] = useState(false);
    const [readFailed, setReadFailed] = useState(false);
    const [excluded, setExcluded] = useState(0);
    const [email, setEmail] = useState(''), [password, setPassword] = useState('');
    const [rows, setRows] = useState<Row[] | null>(null), [signed, setSigned] = useState(false);
    const [views, setViews] = useState<ViewSnapshot>(NO_VIEWS);
    const [registry, setRegistry] = useState<{ rows: RegistryRow[]; status: RegistryState }>({ rows: [], status: 'not-configured' });
    const [busy, setBusy] = useState(true), [status, setStatus] = useState('Checking operator session…');
    const mode = studyAccessMode(location.hostname, location.protocol, import.meta.env.VITE_SUPABASE_URL);
    async function load(action: 'login' | 'refresh' | 'restore') {
        if (!mode || inFlight.current) return;
        inFlight.current = true; setBusy(true); setRows(null); setViews(NO_VIEWS); setRegistry({ rows: [], status: 'not-configured' }); setExcluded(0); setLogoutFailed(false); setReadFailed(false);
        if (action === 'refresh') requested.current = operatorDestination(location.pathname, location.hash);
        let verified = false;
        if (action !== 'refresh') mapSession.current = {};
        setStatus(action === 'refresh' ? 'Refreshing synchronized observations…' : 'Checking operator authorization…');
        try {
            const c = getSupabaseClient();
            if (action === 'login') {
                const { error } = await c.auth.signInWithPassword({ email, password }); setPassword('');
                if (error) throw Error('Sign-in failed');
            } else if (action === 'restore') {
                // Session existence is only a hint; authorization is checked with getUser below.
                const { data, error } = await c.auth.getSession();
                if (error) throw Error('Session check failed');
                if (!data.session) { setSigned(false); route('/login'); setStatus('Sign in as an authorized operator.'); return; }
            }
            const { data: user, error: authError } = await c.auth.getUser();
            if (authError || user.user?.app_metadata.thesis_study_reviewer !== true || user.user.app_metadata.thesis_study_device === true) throw Error('Operator authorization required');
            verified = true;
            const out: Row[] = [];
            for (let start = 0; start <= 10000; start += 500) {
                const { data, error, status: readStatus } = await c.from('study_sf_records').select('recorder_node_id,record_id,record_json,synced_at').order('recorder_node_id').order('record_id').range(start, start + 499);
                if (isAccessFailure(readStatus, error?.code)) { verified = false; throw Error('Operator access could not be verified.'); }
                if (error || !data) throw Error('Study read failed');
                out.push(...(data as Row[]));
                if (out.length > 10000) throw Error('Study limit exceeded; no partial snapshot shown');
                if (data.length < 500) {
                    const view = excludeKnownFixtures(out);
                    // Local historical labs may predate the hosted registry columns. Do not relax their access rules.
                    if (mode === 'hosted') {
                        const entries: RegistryRow[] = [];
                        let registryComplete = false;
                        for (let offset = 0; offset < 10000; offset += 500) {
                            const { data: identityRows, error: identityError, status: identityStatus } = await c.from('vessels').select('id,node_id,vessel_name,registration_serial_no').order('node_id').range(offset, offset + 499);
                            if (isAccessFailure(identityStatus, identityError?.code)) { verified = false; throw Error('Operator registry access could not be verified.'); }
                            if (identityError || !identityRows) break;
                            entries.push(...(identityRows as RegistryRow[]));
                            if (identityRows.length < 500) { registryComplete = true; break; }
                        }
                        setRegistry(registryComplete ? { rows: entries, status: 'ready' } : { rows: [], status: 'unavailable' });
                    }
                    if (mode === 'hosted') {
                        try { setViews(await readRelationalViews(c)); }
                        catch (e) { if (e instanceof ViewAccessError) { verified = false; throw e; } setViews({ status: 'unavailable' }); }
                    }
                    setRows(view.records); setExcluded(view.excluded); setSigned(true); route(action === 'refresh' ? operatorDestination(location.pathname, location.hash) : requested.current);
                    setStatus(`${view.records.length} synchronized non-fixture records; ${view.excluded} known test records excluded.`); return;
                }
            }
            throw Error('Study limit exceeded; no partial snapshot shown');
        } catch (e) {
            if (verified) { setRows(null); setExcluded(0); setSigned(true); route(action === 'refresh' ? operatorDestination(location.pathname, location.hash) : requested.current); setReadFailed(true); setStatus('Unable to retrieve synchronized observations. Retry Refresh.'); return; }
            mapSession.current = {}; setRows(null); setViews(NO_VIEWS); setRegistry({ rows: [], status: 'not-configured' }); setSigned(false); setPassword(''); route('/login');
            setStatus(e instanceof Error ? e.message : 'Study read failed');
            try { const { error } = await getSupabaseClient().auth.signOut(); setLogoutFailed(!!error); } catch {/* protected data already cleared */ }
        } finally { inFlight.current = false; setBusy(false); if (action === 'restore') setStarting(false); }
    }
    useEffect(() => {
        if (started.current) return; started.current = true;
        if (!mode) { route('/login'); setStarting(false); setBusy(false); setStatus('Study access is unavailable for this site or database configuration.'); return; }
        void load('restore');
    }, [mode]);
    useEffect(() => {
        if (signed || busy) return;
        const guard = () => route('/login');
        guard(); window.addEventListener('popstate', guard);
        return () => window.removeEventListener('popstate', guard);
    }, [signed, busy]);
    async function logout() {
        if (inFlight.current) return; inFlight.current = true;
        requested.current = '/dashboard'; mapSession.current = {}; setRows(null); setViews(NO_VIEWS); setRegistry({ rows: [], status: 'not-configured' }); setExcluded(0); setReadFailed(false); setSigned(false); setPassword(''); setBusy(true); route('/login'); setStatus('Signing out…');
        try { const { error } = await getSupabaseClient().auth.signOut(); setLogoutFailed(!!error); setStatus(error ? 'Records cleared; session sign-out failed. Retry sign-out.' : 'Signed out.'); }
        catch { setLogoutFailed(true); setStatus('Records cleared; session sign-out failed.'); }
        finally { inFlight.current = false; setBusy(false); }
    }
    const controls = <><button className="button" disabled={busy} onClick={() => void load('refresh')}>{busy ? 'Refreshing…' : readFailed ? 'Retry' : 'Refresh'}</button> <button className="button" disabled={busy} onClick={() => void logout()}>Sign out</button></>;
    if (mode && signed) return <App studyRows={rows ?? EMPTY_ROWS} relationalViews={views} studyRegistry={registry.rows} registryStatus={registry.status} studyControls={controls} excludedFixtures={excluded} mapSession={mapSession.current} studyPhase={snapshotPhase(busy, readFailed, rows?.length ?? 0)} studyStatus={status} />;
    if (starting && busy) return <div className="operator-startup" role="status" aria-live="polite" aria-busy="true">{slowStart ? <span>Checking operator session…</span> : <span className="startup-sr-only">Checking operator session</span>}</div>;
    return <main style={{ maxWidth: 800, margin: 'auto', padding: 24 }}>
        <div className="page-heading"><div><h1>{busy ? 'Vessels & weather' : signed ? 'Operator dashboard' : 'Operator sign-in'}</h1><p>{mode === 'hosted' ? 'Hosted' : 'Local'} authorized access · synchronized history only.</p></div><a href="/">Home</a></div>
        <section className="panel" style={{ padding: 24 }}>
            {busy ? <p role="status" aria-live="polite">{status}</p> : !mode ? <p role="status">{status}</p> : logoutFailed ? <><p role="status">{status}</p><button className="button" onClick={() => void logout()}>Retry sign-out</button></> : signed ? <><p>No synchronized non-fixture observations yet.</p>{controls}</> : <><form onSubmit={e => { e.preventDefault(); void load('login') }}><label>Email <input type="email" autoComplete="username" value={email} onChange={e => setEmail(e.target.value)} required /></label> <label>Password <input type="password" autoComplete="current-password" value={password} onChange={e => setPassword(e.target.value)} required /></label> <button className="button">Sign in</button></form><p role="status">{status}</p></>}
        </section>
    </main>;
}
