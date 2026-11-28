import { useCallback, useEffect, useMemo, useState, type ReactNode, type MouseEvent, useRef } from 'react';
import { ArrowRight, ArrowUpRight, CircleHelp, Clock3, Cpu, Database, FlaskConical, LayoutDashboard, Radio, MapPin, CloudSun, ShieldAlert } from 'lucide-react';
import { filterReadings, getDemoData, getDeviceStatus, getLatestReading, metrics, metricKeys, dateTime, type Metric } from './lib/data';
import TrendChart from './components/TrendChart';
import InfoDialog from './components/InfoDialog';
import MarineOverview from './features/overview/MarineOverview';
import type { MapSession } from './features/geolocation/VesselMap';

import { studyDashboard } from './lib/studyDashboardData';
import type { RegistryRow, RegistryState } from './features/geolocation/geolocation';
const EMPTY_REGISTRY: RegistryRow[] = [];
import type { Row } from './lib/studySf';
import StudyHistory from './features/records/StudyHistory';
import VesselSelector from './components/VesselSelector';
import WeatherMeasurements from './features/weather/WeatherMeasurements';
import { NO_VIEWS, type ViewSnapshot } from './lib/relationalViews';
import { studyAccessMode } from './features/auth/studyAccessMode';
import { selectionFor, type SnapshotPhase } from './lib/refreshState';
import { sectionAt, sectionFromLocation, sectionPath, sectionLabels, type Section } from './routes';
export default function App({ studyRows, relationalViews = NO_VIEWS, studyRegistry = EMPTY_REGISTRY, registryStatus = 'not-configured', studyControls, excludedFixtures = 0, mapSession: session, studyPhase = 'ready', studyStatus = '' }: { studyRows?: Row[]; relationalViews?: ViewSnapshot; studyRegistry?: RegistryRow[]; registryStatus?: RegistryState; studyControls?: ReactNode; excludedFixtures?: number; mapSession?: MapSession; studyPhase?: SnapshotPhase; studyStatus?: string } = {}) {
    const demo = studyRows === undefined;
    const demoSession = useRef<MapSession>({});
    const mapSession = session ?? demoSession.current;
    const [tilesEnabled, setTilesEnabled] = useState(() => mapSession.tilesEnabled ?? true);
    const canControlTiles = !demo && studyAccessMode(location.hostname, location.protocol, import.meta.env.VITE_SUPABASE_URL) === 'hosted';
    function changeTiles(enabled: boolean) { mapSession.tilesEnabled = enabled; setTilesEnabled(enabled); }
    const [section, setSection] = useState<Section>(() => sectionFromLocation(location.pathname, location.hash, demo));
    useEffect(() => {
        const sync = () => {
            const next = sectionFromLocation(location.pathname, location.hash, demo);
            const path = sectionPath(next, demo);
            if (sectionAt(location.pathname, demo) === undefined && location.pathname !== '/login') { location.reload(); return; }
            if (location.pathname !== path || location.search || location.hash) history.replaceState(null, '', path);
            setSection(next);
        };
        sync(); window.addEventListener('popstate', sync); window.addEventListener('hashchange', sync);
        return () => { window.removeEventListener('popstate', sync); window.removeEventListener('hashchange', sync); };
    }, [demo]);
    function navigate(next: Section) {
        const path = sectionPath(next, demo);
        if (location.pathname !== path || location.search || location.hash) history.pushState(null, '', path);
        setSection(next); window.scrollTo(0, 0);
    }
    function follow(e: MouseEvent<HTMLAnchorElement>, next: Section) {
        if (e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey || e.defaultPrevented) return;
        e.preventDefault(); navigate(next);
    }
    const navItems = [{ key: 'overview', Icon: LayoutDashboard }, { key: 'vessels', Icon: MapPin }, { key: 'weather', Icon: CloudSun }, { key: 'sos', Icon: ShieldAlert }, { key: 'records', Icon: Database }] as const;
    const now = useMemo(() => Date.now(), [studyRows]);
    const study = useMemo(() => studyRows ? studyDashboard(studyRows, now, studyRegistry, registryStatus, relationalViews) : null, [studyRows, now, studyRegistry, registryStatus, relationalViews]);
    const data = useMemo(() => study ?? getDemoData(now), [study, now]);
    const [preferredId, setDeviceId] = useState(() => mapSession?.selectedId ?? data.devices[0]?.id ?? '');
    const deviceId = selectionFor(data.devices, preferredId);
    useEffect(() => { if (deviceId) { if (mapSession) mapSession.selectedId = deviceId; if (preferredId !== deviceId) setDeviceId(deviceId); } }, [mapSession, deviceId, preferredId]);
    const loading = !!study && studyPhase === 'loading';
    const countPending = !!study && (studyPhase === 'loading' || studyPhase === 'error');
    const [hours, setHours] = useState(24);
    const [metric, setMetric] = useState<Metric>('temperature');
    const [help, setHelp] = useState(false);
    const device = data.devices.find(d => d.id === deviceId);
    const readings = filterReadings(data.readings, deviceId, hours, now);
    const latest = study ? study.latestWeather[deviceId] : getLatestReading(data.readings, deviceId, now);
    const onlineCount = data.devices.filter(d => getDeviceStatus(data.readings, d.id, now) === 'online').length;
    const selectDevice = useCallback((id: string) => { setDeviceId(id); }, []);
    return <div className="app-shell">
        <aside className="sidebar">
            <a className="brand" href={sectionPath('overview', demo)} onClick={e => follow(e, 'overview')} aria-label="Acmac dashboard overview"><span className="brand-icon"><Radio size={23} /></span>acmac<span className="brand-period">.</span></a>
            <div className="workspace"><span className="workspace-icon"><Cpu size={19} /></span><span><strong>Barangay Acmac</strong><small>Iligan City · LoRa system</small></span>{!study && <span className="workspace-tag">DEMO</span>}</div>
            <div className="nav-label">COASTAL MONITORING</div>
            <nav aria-label="Main navigation">{navItems.map(({ key, Icon }) => <a key={key} className={`nav-link ${section === key ? 'active' : ''}`} aria-current={section === key ? 'page' : undefined} aria-label={sectionLabels[key]} href={sectionPath(key, demo)} onClick={e => follow(e, key)}><Icon size={18} /> {sectionLabels[key]}{key === 'vessels' && <span className="nav-count" aria-label={countPending ? (loading ? 'Loading vessel count' : 'Vessel count unavailable') : undefined}>{countPending ? '—' : data.devices.length}</span>}</a>)}</nav>
            <div className="sidebar-bottom"><div className="connection-card"><span className="connection-orbit"><Radio size={22} /></span><strong>{study ? 'Synchronized observations' : 'Connected across the bay.'}</strong><p>Vessel locations and weather<br />for the Acmac fishing community.</p><span className="sample-pill"><span /> {study ? 'Synchronized history' : 'Research demonstration'}</span></div><button className="help-button" onClick={() => setHelp(true)}><CircleHelp size={18} /> About this dashboard <ArrowUpRight size={14} /></button></div>
        </aside>
        <div className="main-shell">
            <main id="dashboard-content" data-section={section}>
                <header className="page-heading compact-page-heading"><div><div className="eyebrow"><span /> LORA GEOLOCATION & WEATHER ASSISTANCE</div><h1>{section === 'overview' ? 'Vessels & weather' : sectionLabels[section]}</h1><p>For small-scale fishermen and shore monitoring in Barangay Acmac, Iligan City, Lanao del Norte.</p></div><div className="topbar-right page-actions"><span className="public-label"><span /> {study ? 'Authorized operator' : 'Local demo'}</span>{study ? <div className="study-header-controls" aria-label="Dashboard actions">{studyControls}</div> : null}</div></header>
                {study ? <div className="demo-banner study-refresh-banner"><span className="demo-icon"><Database size={19} /></span><div className="study-banner-copy"><strong>Synchronized observations</strong><div className="study-status-slot"><span role="status" aria-live="polite">{studyPhase === 'ready' ? `${study.records.length} synchronized non-fixture records; ${excludedFixtures} known synthetic records excluded from this view and export.` : studyPhase === 'empty' ? 'No synchronized non-fixture observations available.' : studyStatus}</span></div><div>Bench observations; not live LoRa tracking or rescue dispatch.</div></div></div> : <div className="demo-banner"><span className="demo-icon"><FlaskConical size={19} /></span><div><strong>Research prototype · sample data.</strong> <span>Vessel positions, weather and SOS events are fictional. No live hardware is connected.</span></div><span className="demo-badge">DEMO MODE</span> {studyAccessMode(location.hostname, location.protocol, import.meta.env.VITE_SUPABASE_URL) && <a href="/?study=1">Sign in to study data</a>}</div>}
                {section !== 'sos' && section !== 'records' && <div className="filter-bar"><div className="filter-left"><VesselSelector key={section} devices={data.devices} selected={deviceId} onSelect={selectDevice} placeholder={loading ? 'Loading vessels…' : studyPhase === 'error' ? 'Vessels unavailable' : 'No vessels available'} /></div><span className="snapshot"><Clock3 size={14} /> {study ? 'Review snapshot' : 'Demo snapshot'} · {dateTime(now)} PHT</span></div>}
                {section === 'weather' && <WeatherMeasurements latest={latest} loading={loading} countPending={countPending} study={!!study} />}
                {section !== 'records' && <MarineOverview views={relationalViews} devices={data.devices} readings={data.readings} sosEvents={data.sosEvents} selectedId={deviceId} now={now} onSelect={section === 'sos' ? (id) => { selectDevice(id); navigate('overview') } : selectDevice} study={study ?? undefined} mapSession={mapSession} tilesEnabled={tilesEnabled} studyPhase={studyPhase} section={section} onViewSos={e => follow(e, 'sos')} />}
                {(section === 'weather' || section === 'vessels') && <div className="middle-grid section-single">
                    {section === 'weather' && <section className="panel trends"><div className="panel-heading"><div><h2>Reading trends <span className="live-label">{study ? 'SYNCED' : 'SAMPLE'}</span></h2><p>Explore the selected vessel’s sensor history.</p></div><div className="range-switch" aria-label="Reading trends time range">{[24, 168].map(h => <button key={h} className={hours === h ? 'selected' : ''} aria-pressed={hours === h} onClick={() => { setHours(h) }}>{h === 24 ? 'Last 24 hours' : 'Last 7 days'}</button>)}</div></div><div className="metric-tabs" aria-label="Chart metric">{metricKeys.filter(key => !study || key !== 'signal').map(key => <button key={key} className={metric === key ? 'selected' : ''} aria-pressed={metric === key} onClick={() => setMetric(key)}><span style={{ background: metrics[key].color }} />{metrics[key].label}</button>)}</div><div className="chart-unit">{metrics[metric].label} ({metrics[metric].unit})</div>{study && studyPhase !== 'ready' ? <div className="chart-empty" role="status">{loading ? 'Loading reading history…' : studyPhase === 'error' ? 'Unable to load reading history.' : 'No synchronized reading history.'}</div> : <TrendChart readings={readings} metric={metric} synchronized={!!study} />}<div className="chart-footer"><span><span className="legend-line" style={{ background: metrics[metric].color }} />{loading ? 'Loading vessel…' : countPending ? 'Vessel unavailable' : device?.name ?? 'No vessel selected'}</span><span>{countPending ? '—' : readings.length.toLocaleString()} {study ? 'timed weather points' : 'sample packets'} · PHT</span></div></section>}
                    {section === 'vessels' && <section className="panel device-panel" id="devices"><div className="panel-heading"><div><h2>Vessel nodes <span className="number-badge" aria-label={countPending ? (loading ? 'Loading vessel count' : 'Vessel count unavailable') : undefined}>{countPending ? '—' : data.devices.length}</span></h2><p>{study ? (loading ? 'Loading vessel nodes…' : studyPhase === 'error' ? 'Unable to load vessel nodes.' : `${data.devices.length} recording vessels`) : `${onlineCount} reporting in this demo snapshot`}</p></div><Cpu size={18} className="muted" /></div><div className="device-list">{!data.devices.length && <p className="data-state-note">{loading ? 'Loading vessel nodes…' : studyPhase === 'error' ? 'Unable to load vessel nodes.' : 'No vessel nodes available.'}</p>}{data.devices.map(d => { const isOnline = getDeviceStatus(data.readings, d.id, now) === 'online'; return <button key={d.id} className={`device-row ${deviceId === d.id ? 'current' : ''}`} onClick={() => selectDevice(d.id)} aria-pressed={deviceId === d.id}><span className={`device-symbol ${!study && isOnline ? '' : 'is-offline'}`}><Cpu size={21} /></span><span className="device-detail"><strong>{d.name}</strong><small>{d.id} <span>·</span> {study ? (d.registeredName ?? 'Synchronized observations') : isOnline ? 'Online' : 'Offline'}</small></span>{!study && <span className={`status-dot ${isOnline ? '' : 'offline'}`} />}</button> })}</div><div className="device-note"><Radio size={16} /><p>GPS + weather over LoRa.<br /><span>{study ? 'Recorded observations, not live links.' : 'All vessel nodes shown here are examples.'}</span></p></div><a className="panel-link" href={sectionPath('records', demo)} onClick={e => follow(e, 'records')}>Explore vessel logs <ArrowRight size={15} /></a></section>}
                </div>}
                {section === 'records' && <StudyHistory rows={study?.records} readings={data.readings} samples={data.sosEvents} devices={data.devices} phase={studyPhase} views={relationalViews} />}
                <div className="project-description">Development of a LoRa-Based Geolocation and Weather Assisting System for Small-Scale Fishermen in Barangay Acmac, Iligan City, Lanao del Norte</div>
                <footer className="page-footer"><span><Radio size={14} /> Acmac fisheries research <span className="footer-dot">·</span> LoRa geolocation &amp; weather assistance</span><span>{study ? 'Synchronized study data' : 'Sample data only'} <span className="footer-dot">·</span> PHT (UTC+8)</span></footer>
            </main>
        </div>
        {help && <InfoDialog study={!!study} basemapEnabled={tilesEnabled} onBasemapChange={canControlTiles ? changeTiles : undefined} onClose={() => setHelp(false)} />}
    </div>;
}
