import { useCallback, useEffect, useMemo, useState, type ReactNode, type MouseEvent, useRef } from 'react';
import { Activity, ArrowDownToLine, ArrowRight, ArrowUpRight, ChevronDown, ChevronLeft, ChevronRight, CircleHelp, Clock3, Cpu, Database, Droplets, FlaskConical, LayoutDashboard, Radio, Search, Signal, Thermometer, Gauge, MapPin, CloudSun, ShieldAlert, X } from 'lucide-react';
import { filterReadings, getDemoData, getDeviceStatus, getLatestReading, metrics, metricKeys, dateTime, weatherLabel, type Metric } from './data';
import TrendChart from './TrendChart';
import InfoDialog from './InfoDialog';
import MarineOverview from './MarineOverview';
import type {MapSession} from './VesselMap';

import {studyDashboard} from './studyDashboardData';
import type {RegistryRow,RegistryState} from './geolocation';
const EMPTY_REGISTRY:RegistryRow[]=[];
import type {Row} from './studySf';
import StudyHistory,{exportStudy} from './StudyHistory';
import {NO_VIEWS,type ViewSnapshot} from './relationalViews';
import {studyAccessMode} from './studyAccessMode';
import {selectionFor, type SnapshotPhase} from './refreshState';
import {sectionAt,sectionFromLocation,sectionPath,sectionLabels,type Section} from './routes';
// A disclosure with ordinary buttons, rather than an incomplete ARIA listbox.
function VesselSelector({devices,selected,onSelect,placeholder}:{devices:{id:string;name:string}[];selected:string;onSelect:(id:string)=>void;placeholder:string}) {
 const [open,setOpen]=useState(false);
 const root=useRef<HTMLDivElement>(null),trigger=useRef<HTMLButtonElement>(null);
 const disabled=!devices.length;
 useEffect(()=>{if(disabled)setOpen(false);},[disabled]);
 useEffect(()=>{
  if(!open)return;
  const outside=(e:PointerEvent)=>{if(e.target instanceof Node&&!root.current?.contains(e.target))setOpen(false);};
  document.addEventListener('pointerdown',outside);
  return ()=>document.removeEventListener('pointerdown',outside);
 },[open]);
 const name=devices.find(d=>d.id===selected)?.name??placeholder;
 return <div className="vessel-picker" ref={root} onBlur={e=>{if(!e.currentTarget.contains(e.relatedTarget as Node|null))setOpen(false);}} onKeyDown={e=>{if(e.key==='Escape'&&open){e.preventDefault();setOpen(false);trigger.current?.focus();}}}>
  <button type="button" ref={trigger} className="vessel-picker-trigger" aria-label={`Select vessel: ${name}`} aria-expanded={open&&!disabled} disabled={disabled} onClick={()=>setOpen(v=>!v)}><Cpu size={16}/><span>{name}</span><ChevronDown size={14}/></button>
  {open&&!disabled&&<div className="vessel-picker-options" aria-label="Vessel choices">{devices.map(d=><button type="button" key={d.id} aria-pressed={d.id===selected} onClick={()=>{onSelect(d.id);setOpen(false);trigger.current?.focus();}}>{d.name}{d.id===selected&&<span aria-hidden="true">✓</span>}</button>)}</div>}
 </div>;
}
export default function App({studyRows,relationalViews=NO_VIEWS,studyRegistry=EMPTY_REGISTRY,registryStatus='not-configured',studyControls,excludedFixtures=0,mapSession:session,studyPhase='ready',studyStatus=''}:{studyRows?:Row[];relationalViews?:ViewSnapshot;studyRegistry?:RegistryRow[];registryStatus?:RegistryState;studyControls?:ReactNode;excludedFixtures?:number;mapSession?:MapSession;studyPhase?:SnapshotPhase;studyStatus?:string} = {}) {
 const demo=studyRows===undefined;
 const demoSession=useRef<MapSession>({});
 const mapSession=session??demoSession.current;
 const [tilesEnabled,setTilesEnabled]=useState(()=>mapSession.tilesEnabled??true);
 const canControlTiles=!demo&&studyAccessMode(location.hostname,location.protocol,import.meta.env.VITE_SUPABASE_URL)==='hosted';
 function changeTiles(enabled:boolean){mapSession.tilesEnabled=enabled;setTilesEnabled(enabled);}
 const [section,setSection]=useState<Section>(()=>sectionFromLocation(location.pathname,location.hash,demo));
 useEffect(()=>{
  const sync=()=>{
   const next=sectionFromLocation(location.pathname,location.hash,demo);
   const path=sectionPath(next,demo);
   if(sectionAt(location.pathname,demo)===undefined && location.pathname!=='/login'){location.reload();return;}
   if(location.pathname!==path||location.search||location.hash) history.replaceState(null,'',path);
   setSection(next);
  };
  sync();window.addEventListener('popstate',sync);window.addEventListener('hashchange',sync);
  return ()=>{window.removeEventListener('popstate',sync);window.removeEventListener('hashchange',sync);};
 },[demo]);
 function navigate(next:Section){
  const path=sectionPath(next,demo);
  if(location.pathname!==path||location.search||location.hash)history.pushState(null,'',path);
  setSection(next);window.scrollTo(0,0);
 }
 function follow(e:MouseEvent<HTMLAnchorElement>,next:Section){
  if(e.button!==0||e.metaKey||e.ctrlKey||e.shiftKey||e.altKey||e.defaultPrevented)return;
  e.preventDefault();navigate(next);
 }
 const navItems=[{key:'overview',Icon:LayoutDashboard},{key:'vessels',Icon:MapPin},{key:'weather',Icon:CloudSun},{key:'sos',Icon:ShieldAlert},{key:'records',Icon:Database}] as const;
 const now = useMemo(() => Date.now(),[studyRows]);
 const study = useMemo(()=>studyRows ? studyDashboard(studyRows,now,studyRegistry,registryStatus) : null,[studyRows,now,studyRegistry,registryStatus]);
 const data = useMemo(()=>study ?? getDemoData(now),[study,now]);
 const [preferredId,setDeviceId] = useState(() => mapSession?.selectedId ?? data.devices[0]?.id ?? '');
 const deviceId=selectionFor(data.devices,preferredId);
 useEffect(()=>{if(deviceId){if(mapSession)mapSession.selectedId=deviceId;if(preferredId!==deviceId)setDeviceId(deviceId);}},[mapSession,deviceId,preferredId]);
 const loading=!!study&&studyPhase==='loading';
 const countPending=!!study&&(studyPhase==='loading'||studyPhase==='error');
 const [hours, setHours] = useState(24);
 const [metric, setMetric] = useState<Metric>('temperature');
 const [query, setQuery] = useState('');
 const [page, setPage] = useState(0);
 const [help, setHelp] = useState(false);
 const device = data.devices.find(d => d.id === deviceId);
 const readings = filterReadings(data.readings, deviceId, hours, now);
 const latest = study ? study.latestWeather[deviceId] : getLatestReading(data.readings,deviceId,now);
 const filtered = [...readings].reverse().filter(r => `${dateTime(r.timestamp)} ${device?.name??''} ${deviceId} ${r.temperature} ${r.humidity} ${r.pressure} ${r.signal}`.toLowerCase().includes(query.toLowerCase()));
 const pages = Math.max(1, Math.ceil(filtered.length/6));
 const visible = filtered.slice(page*6,page*6+6);
 const onlineCount = data.devices.filter(d => getDeviceStatus(data.readings,d.id,now)==='online').length;
 const selectDevice = useCallback((id: string) => { setDeviceId(id); setPage(0); }, []);
 function exportCsv() {
  if(study){exportStudy(study.records.filter(r=>String(r.recorder_node_id)===deviceId));return;}
  const csv = ['timestamp_utc,vessel_id,temperature_c,humidity_percent,pressure_hpa,pressure_trend_hpa,lora_rssi_dbm,latitude,longitude,gps_fix,battery_percent,shore_distance_m,weather_state', ...filtered.map(r => [new Date(r.timestamp).toISOString(),r.deviceId,r.temperature??'',r.humidity??'',r.pressure??'',r.pressureTrend??'',r.signal,r.latitude??'',r.longitude??'',r.gpsFix,r.battery,r.shoreDistance,r.weatherState].join(','))].join('\n');
  const url = URL.createObjectURL(new Blob([csv], {type:'text/csv;charset=utf-8;'}));
  const a = document.createElement('a'); a.href=url; a.download=`acmac-demo-${deviceId}-${hours}h.csv`; a.click(); setTimeout(() => URL.revokeObjectURL(url),1000);
 }
 return <div className="app-shell">
  <aside className="sidebar">
   <a className="brand" href={sectionPath('overview',demo)} onClick={e=>follow(e,'overview')} aria-label="Acmac dashboard overview"><span className="brand-icon"><Radio size={23}/></span>acmac<span className="brand-period">.</span></a>
   <div className="workspace"><span className="workspace-icon"><Cpu size={19}/></span><span><strong>Barangay Acmac</strong><small>Iligan City · LoRa system</small></span>{!study&&<span className="workspace-tag">DEMO</span>}</div>
   <div className="nav-label">COASTAL MONITORING</div>
   <nav aria-label="Main navigation">{navItems.map(({key,Icon})=><a key={key} className={`nav-link ${section===key?'active':''}`} aria-current={section===key?'page':undefined} aria-label={sectionLabels[key]} href={sectionPath(key,demo)} onClick={e=>follow(e,key)}><Icon size={18}/> {sectionLabels[key]}{key==='vessels'&&<span className="nav-count" aria-label={countPending?(loading?'Loading vessel count':'Vessel count unavailable'):undefined}>{countPending?'—':data.devices.length}</span>}</a>)}</nav>
   <div className="sidebar-bottom"><div className="connection-card"><span className="connection-orbit"><Radio size={22}/></span><strong>{study?'Synchronized observations':'Connected across the bay.'}</strong><p>Vessel locations and weather<br/>for the Acmac fishing community.</p><span className="sample-pill"><span/> {study?'Synchronized history':'Research demonstration'}</span></div><button className="help-button" onClick={() => setHelp(true)}><CircleHelp size={18}/> About this dashboard <ArrowUpRight size={14}/></button></div>
  </aside>
  <div className="main-shell">
   <main id="dashboard-content" data-section={section}>
    <header className="page-heading compact-page-heading"><div><div className="eyebrow"><span/> LORA GEOLOCATION & WEATHER ASSISTANCE</div><h1>{section==='overview'?'Vessels & weather':sectionLabels[section]}</h1><p>For small-scale fishermen and shore monitoring in Barangay Acmac, Iligan City, Lanao del Norte.</p></div><div className="topbar-right page-actions"><span className="public-label"><span/> {study?'Authorized operator':'Local demo'}</span>{study?<div className="study-header-controls" aria-label="Dashboard actions">{studyControls}</div>:null}</div></header>
    {study?<div className="demo-banner study-refresh-banner"><span className="demo-icon"><Database size={19}/></span><div className="study-banner-copy"><strong>Synchronized observations</strong><div className="study-status-slot"><span role="status" aria-live="polite">{studyPhase==='ready'?`${study.records.length} synchronized non-fixture records; ${excludedFixtures} known synthetic records excluded from this view and export.`:studyPhase==='empty'?'No synchronized non-fixture observations available.':studyStatus}</span></div><div>Bench observations; not live LoRa tracking or rescue dispatch.</div></div></div>:<div className="demo-banner"><span className="demo-icon"><FlaskConical size={19}/></span><div><strong>Research prototype · sample data.</strong> <span>Vessel positions, weather and SOS events are fictional. No live hardware is connected.</span></div><span className="demo-badge">DEMO MODE</span> {studyAccessMode(location.hostname,location.protocol,import.meta.env.VITE_SUPABASE_URL)&&<a href="/?study=1">Sign in to study data</a>}</div>}
    {section!=='sos'&&section!=='records'&&<div className="filter-bar"><div className="filter-left"><VesselSelector key={section} devices={data.devices} selected={deviceId} onSelect={selectDevice} placeholder={loading?'Loading vessels…':studyPhase==='error'?'Vessels unavailable':'No vessels available'}/></div><span className="snapshot"><Clock3 size={14}/> {study?'Review snapshot':'Demo snapshot'} · {dateTime(now)} PHT</span></div>}
    {section==='weather'&&<section className="weather-measurements" aria-label="Latest sensor samples" aria-busy={loading}>
     <div className="stats-grid weather-measurement-grid">
      {(['temperature','humidity','pressure','pressureTrend'] as const).map(key=>{
       const trend=key==='pressureTrend';
       const Icon=key==='temperature'?Thermometer:key==='humidity'?Droplets:trend?Activity:Gauge;
       const label=trend?'Pressure trend':metrics[key].label;
       const unit=trend?'hPa':metrics[key].unit;
       const value=latest?.[key];
       const valid=!countPending&&typeof value==='number'&&Number.isFinite(value);
       return <article className={'stat-card '+key} key={key}><div className="stat-top"><span>{label}</span><span className="stat-icon"><Icon size={18}/></span></div><div className="stat-value">{loading?<span className="value-skeleton" aria-label="Loading sample"/>:valid?(trend&&value>0?'+':'')+value.toFixed(1):'—'}<span>{unit}</span></div><div className="stat-foot">{loading?'Loading sample…':valid?(study?'Recorded sensor sample':'Fictional sensor sample'):'Unavailable'}</div></article>;
      })}
     </div>
     <div className="weather-sample-context"><span><Clock3 size={14}/>{loading?'Loading observation…':countPending||!latest||!Number.isFinite(latest.timestamp)?'Observation time unavailable':(study?'Captured: ':'Sample: ')+dateTime(latest.timestamp)+' PHT'}</span><span className={'weather-state state-'+(countPending?-1:latest?.weatherState??-1)}>{loading?'Loading…':countPending||!latest?'Unavailable':weatherLabel(latest.weatherState)}</span></div>
     <p className="weather-measurement-note">Recorded sensor state only; not an official forecast or a guarantee of safe conditions.</p>
    </section>}
    {section!=='records'&&<MarineOverview views={relationalViews} devices={data.devices} readings={data.readings} sosEvents={data.sosEvents} selectedId={deviceId} now={now} onSelect={section==='sos'?(id)=>{selectDevice(id);navigate('overview')}:selectDevice} study={study??undefined} mapSession={mapSession} tilesEnabled={tilesEnabled} studyPhase={studyPhase} section={section} onViewSos={e=>follow(e,'sos')}/> }
    {(section==='weather'||section==='vessels')&&<div className="middle-grid section-single">
     {section==='weather'&&<section className="panel trends"><div className="panel-heading"><div><h2>Reading trends <span className="live-label">{study?'SYNCED':'SAMPLE'}</span></h2><p>Explore the selected vessel’s sensor history.</p></div><div className="range-switch" aria-label="Reading trends time range">{[24,168].map(h => <button key={h} className={hours===h?'selected':''} aria-pressed={hours===h} onClick={() => {setHours(h);setPage(0)}}>{h===24?'Last 24 hours':'Last 7 days'}</button>)}</div></div><div className="metric-tabs" aria-label="Chart metric">{metricKeys.filter(key=>!study||key!=='signal').map(key => <button key={key} className={metric===key?'selected':''} aria-pressed={metric===key} onClick={() => setMetric(key)}><span style={{background:metrics[key].color}}/>{metrics[key].label}</button>)}</div><div className="chart-unit">{metrics[metric].label} ({metrics[metric].unit})</div>{study&&studyPhase!=='ready'?<div className="chart-empty" role="status">{loading?'Loading reading history…':studyPhase==='error'?'Unable to load reading history.':'No synchronized reading history.'}</div>:<TrendChart readings={readings} metric={metric} synchronized={!!study}/>}<div className="chart-footer"><span><span className="legend-line" style={{background:metrics[metric].color}}/>{loading?'Loading vessel…':countPending?'Vessel unavailable':device?.name??'No vessel selected'}</span><span>{countPending?'—':readings.length.toLocaleString()} {study?'timed weather points':'sample packets'} · PHT</span></div></section>}
     {section==='vessels'&&<section className="panel device-panel" id="devices"><div className="panel-heading"><div><h2>Vessel nodes <span className="number-badge" aria-label={countPending?(loading?'Loading vessel count':'Vessel count unavailable'):undefined}>{countPending?'—':data.devices.length}</span></h2><p>{study?(loading?'Loading vessel nodes…':studyPhase==='error'?'Unable to load vessel nodes.':`${data.devices.length} recording vessels`):`${onlineCount} reporting in this demo snapshot`}</p></div><Cpu size={18} className="muted"/></div><div className="device-list">{!data.devices.length&&<p className="data-state-note">{loading?'Loading vessel nodes…':studyPhase==='error'?'Unable to load vessel nodes.':'No vessel nodes available.'}</p>}{data.devices.map(d => { const isOnline=getDeviceStatus(data.readings,d.id,now)==='online'; return <button key={d.id} className={`device-row ${deviceId===d.id?'current':''}`} onClick={() => selectDevice(d.id)} aria-pressed={deviceId===d.id}><span className={`device-symbol ${!study&&isOnline?'':'is-offline'}`}><Cpu size={21}/></span><span className="device-detail"><strong>{d.name}</strong><small>{d.id} <span>·</span> {study?(d.registeredName??'Synchronized observations'):isOnline?'Online':'Offline'}</small></span>{!study&&<span className={`status-dot ${isOnline?'':'offline'}`}/>}</button> })}</div><div className="device-note"><Radio size={16}/><p>GPS + weather over LoRa.<br/><span>{study?'Recorded observations, not live links.':'All vessel nodes shown here are examples.'}</span></p></div><a className="panel-link" href={sectionPath('records',demo)} onClick={e=>follow(e,'records')}>Explore vessel logs <ArrowRight size={15}/></a></section>}
    </div>}
    {section==='records'&&<StudyHistory rows={study?.records} readings={data.readings} samples={data.sosEvents} devices={data.devices} phase={studyPhase} views={relationalViews}/>}
    <div className="project-description">Development of a LoRa-Based Geolocation and Weather Assisting System for Small-Scale Fishermen in Barangay Acmac, Iligan City, Lanao del Norte</div>
    <footer className="page-footer"><span><Radio size={14}/> Acmac fisheries research <span className="footer-dot">·</span> LoRa geolocation &amp; weather assistance</span><span>{study?'Synchronized study data':'Sample data only'} <span className="footer-dot">·</span> PHT (UTC+8)</span></footer>
   </main>
  </div>
  {help && <InfoDialog study={!!study} basemapEnabled={tilesEnabled} onBasemapChange={canControlTiles?changeTiles:undefined} onClose={() => setHelp(false)} />}
 </div>;
}
