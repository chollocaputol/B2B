import { useCallback, useState } from 'react';
import { Activity, ArrowDownToLine, ArrowRight, ArrowUpRight, BellRing, ChevronDown, ChevronLeft, ChevronRight, CircleHelp, Clock3, Cpu, Database, Droplets, FlaskConical, LayoutDashboard, Radio, Search, Signal, Thermometer, Gauge, Anchor, CloudSun, Wifi, WifiOff, X } from 'lucide-react';
import { filterReadings, getDemoData, getDeviceStatus, getLatestReading, metrics, metricKeys, dateTime, type Metric } from './data';
import TrendChart from './TrendChart';
import InfoDialog from './InfoDialog';
import MarineOverview from './MarineOverview';

export default function App() {
 const [now] = useState(() => Date.now());
 const [data] = useState(() => getDemoData(now));
 const [deviceId, setDeviceId] = useState(data.devices[0].id);
 const [hours, setHours] = useState(24);
 const [metric, setMetric] = useState<Metric>('temperature');
 const [query, setQuery] = useState('');
 const [page, setPage] = useState(0);
 const [help, setHelp] = useState(false);
 const device = data.devices.find(d => d.id === deviceId)!;
 const readings = filterReadings(data.readings, deviceId, hours, now);
 const latest = getLatestReading(data.readings,deviceId,now);
 const online = getDeviceStatus(data.readings, deviceId, now) === 'online';
 const filtered = [...readings].reverse().filter(r => `${dateTime(r.timestamp)} ${device.name} ${device.id} ${r.temperature} ${r.humidity} ${r.pressure} ${r.signal}`.toLowerCase().includes(query.toLowerCase()));
 const pages = Math.max(1, Math.ceil(filtered.length/6));
 const visible = filtered.slice(page*6,page*6+6);
 const onlineCount = data.devices.filter(d => getDeviceStatus(data.readings,d.id,now)==='online').length;
 const selectDevice = useCallback((id: string) => { setDeviceId(id); setPage(0); }, []);
 function exportCsv() {
  const csv = ['timestamp_utc,vessel_id,temperature_c,humidity_percent,pressure_hpa,pressure_trend_hpa,lora_rssi_dbm,latitude,longitude,gps_fix,battery_percent,shore_distance_m,weather_state', ...filtered.map(r => [new Date(r.timestamp).toISOString(),r.deviceId,r.temperature??'',r.humidity??'',r.pressure??'',r.pressureTrend??'',r.signal,r.latitude??'',r.longitude??'',r.gpsFix,r.battery,r.shoreDistance,r.weatherState].join(','))].join('\n');
  const url = URL.createObjectURL(new Blob([csv], {type:'text/csv;charset=utf-8;'}));
  const a = document.createElement('a'); a.href=url; a.download=`acmac-demo-${deviceId}-${hours}h.csv`; a.click(); setTimeout(() => URL.revokeObjectURL(url),1000);
 }
 return <div className="app-shell">
  <aside className="sidebar">
   <a className="brand" href="#overview" aria-label="Acmac dashboard overview"><span className="brand-icon"><Radio size={23}/></span>acmac<span className="brand-period">.</span></a>
   <div className="workspace"><span className="workspace-icon"><Cpu size={19}/></span><span><strong>Barangay Acmac</strong><small>Iligan City · LoRa system</small></span><span className="workspace-tag">PUBLIC</span></div>
   <div className="nav-label">COASTAL MONITORING</div>
   <nav aria-label="Main navigation"><a className="nav-link active" href="#overview"><LayoutDashboard size={18}/> Overview</a><a className="nav-link" href="#devices"><Anchor size={18}/> Vessels <span className="nav-count">3</span></a><a className="nav-link" href="#weather"><CloudSun size={18}/> Weather &amp; SOS</a><a className="nav-link" href="#readings"><Database size={18}/> Telemetry history</a></nav>
   <div className="sidebar-bottom"><div className="connection-card"><span className="connection-orbit"><Radio size={22}/></span><strong>Connected across the bay.</strong><p>Vessel locations and weather<br/>for the Acmac fishing community.</p><span className="sample-pill"><span/> Research demonstration</span></div><button className="help-button" onClick={() => setHelp(true)}><CircleHelp size={18}/> About this dashboard <ArrowUpRight size={14}/></button><div className="sidebar-footer"><span className="avatar">FL</span><span><strong>Fishermen & shore teams</strong><small>Read-only access</small></span><span className="public-dot"/></div></div>
  </aside>
  <div className="main-shell">
   <header className="topbar"><div className="breadcrumb">Acmac <ChevronRight size={13}/><span>Overview</span></div><div className="topbar-right"><span className="public-label"><span/> Public dashboard</span><span className="topbar-divider"/><button className="icon-button" aria-label="About demo notifications" onClick={() => setHelp(true)}><BellRing size={18}/></button><span className="avatar small-avatar">FL</span></div></header>
   <main id="overview">
    <div className="page-heading"><div><div className="eyebrow"><span/> LORA GEOLOCATION & WEATHER ASSISTANCE</div><h1>Vessels &amp; weather</h1><p>For small-scale fishermen and shore monitoring in Barangay Acmac, Iligan City, Lanao del Norte.</p></div><button className="button export-main" onClick={exportCsv}><ArrowDownToLine size={16}/> Export data</button></div>
    <div className="demo-banner"><span className="demo-icon"><FlaskConical size={19}/></span><div><strong>Research prototype · sample data.</strong> <span>Vessel positions, weather and SOS events are fictional. No live hardware is connected.</span></div><span className="demo-badge">DEMO MODE</span></div>
    <div className="filter-bar"><div className="filter-left"><label className="select-wrap"><Cpu size={16}/><select aria-label="Select vessel" value={deviceId} onChange={e => selectDevice(e.target.value)}>{data.devices.map(d => <option key={d.id} value={d.id}>{d.name}</option>)}</select><ChevronDown size={14}/></label><span className="filter-separator"/><div className="range-switch" aria-label="Time range">{[24,168].map(h => <button key={h} className={hours===h?'selected':''} aria-pressed={hours===h} onClick={() => {setHours(h);setPage(0)}}>{h===24?'Last 24 hours':'Last 7 days'}</button>)}</div></div><span className="snapshot"><Clock3 size={14}/> Demo snapshot · {dateTime(now)} PHT</span></div>
    <section className="stats-grid" aria-label="Latest sensor samples">
     {(['temperature','humidity','pressure'] as Metric[]).map(key => { const Icon = key==='temperature'?Thermometer:key==='humidity'?Droplets:Gauge; return <article className={`stat-card ${key}`} key={key}><div className="stat-top"><span>{metrics[key].label}</span><span className="stat-icon"><Icon size={18}/></span></div><div className="stat-value">{latest?.[key]!=null ? latest[key]!.toFixed(1) : '—'}<span>{metrics[key].unit}</span></div><div className="stat-foot"><span className="tiny-dot"/> Latest sensor sample <Activity size={39} strokeWidth={1.3}/></div></article> })}
     <article className="stat-card"><div className="stat-top"><span>LoRa link status</span><span className={`stat-icon ${online?'':'muted-icon'}`}>{online?<Wifi size={18}/>:<WifiOff size={18}/>}</span></div><div className="status-value"><span className={`status-dot ${online?'':'offline'}`}/>{online?'Online':'Offline'}</div><div className="stat-foot">{online?'Packet received in demo':'Last packet over 15 min ago'}<span className="device-code">{device.id}</span></div></article>
    </section>
    <MarineOverview devices={data.devices} readings={data.readings} sosEvents={data.sosEvents} selectedId={deviceId} now={now} onSelect={selectDevice}/>
    <div className="middle-grid">
     <section className="panel trends"><div className="panel-heading"><div><h2>Reading trends <span className="live-label">SAMPLE</span></h2><p>Explore the selected vessel’s sensor history.</p></div><span className="chart-range">{hours===24?'24-hour':'7-day'} view</span></div><div className="metric-tabs" aria-label="Chart metric">{metricKeys.map(key => <button key={key} className={metric===key?'selected':''} aria-pressed={metric===key} onClick={() => setMetric(key)}><span style={{background:metrics[key].color}}/>{metrics[key].label}</button>)}</div><div className="chart-unit">{metrics[metric].label} ({metrics[metric].unit})</div><TrendChart readings={readings} metric={metric}/><div className="chart-footer"><span><span className="legend-line" style={{background:metrics[metric].color}}/>{device.name}</span><span>{readings.length.toLocaleString()} sample packets · PHT</span></div></section>
     <section className="panel device-panel" id="devices"><div className="panel-heading"><div><h2>Vessel nodes <span className="number-badge">3</span></h2><p>{onlineCount} reporting in this demo snapshot</p></div><Cpu size={18} className="muted"/></div><div className="device-list">{data.devices.map(d => { const isOnline=getDeviceStatus(data.readings,d.id,now)==='online'; return <button key={d.id} className={`device-row ${deviceId===d.id?'current':''}`} onClick={() => selectDevice(d.id)} aria-pressed={deviceId===d.id}><span className={`device-symbol ${isOnline?'':'is-offline'}`}><Cpu size={21}/></span><span className="device-detail"><strong>{d.name}</strong><small>{d.id} <span>·</span> {isOnline?'Online':'Offline'}</small></span><span className={`status-dot ${isOnline?'':'offline'}`}/></button> })}</div><div className="device-note"><Radio size={16}/><p>GPS + weather over LoRa.<br/><span>All vessel nodes shown here are examples.</span></p></div><a className="panel-link" href="#readings">Explore vessel telemetry <ArrowRight size={15}/></a></section>
    </div>
    <section className="panel readings-panel" id="readings"><div className="readings-heading"><div><h2>Telemetry history <span className="number-badge">{readings.length}</span></h2><p>Last received samples · Philippine Standard Time (UTC+8).</p></div><div className="table-actions"><label className="search-wrap"><Search size={16}/><input aria-label="Search readings" placeholder="Search readings…" value={query} onChange={e => {setQuery(e.target.value);setPage(0)}}/>{query&&<button className="clear-search" aria-label="Clear search" onClick={() => {setQuery('');setPage(0)}}><X size={13}/></button>}</label><button className="icon-button table-export" aria-label="Export filtered readings as CSV" onClick={exportCsv}><ArrowDownToLine size={17}/></button></div></div><div className="table-scroll"><table><thead><tr><th>Timestamp <ChevronDown size={12}/></th><th>Vessel</th><th>Temperature</th><th>Humidity</th><th>Pressure</th><th>LoRa RSSI</th><th>Source</th></tr></thead><tbody>{visible.map(r => <tr key={`${r.deviceId}-${r.timestamp}`}><td><Clock3 size={13}/>{dateTime(r.timestamp)}</td><td><span className="table-device-dot"/>{device.name}</td><td>{r.temperature!=null?<>{r.temperature.toFixed(1)} <span>°C</span></>:<span>Unavailable</span>}</td><td>{r.humidity!=null?<>{r.humidity.toFixed(1)} <span>%</span></>:<span>Unavailable</span>}</td><td>{r.pressure!=null?<>{r.pressure.toFixed(1)} <span>hPa</span></>:<span>Unavailable</span>}</td><td><Signal size={13}/>{r.signal} <span>dBm</span></td><td><span className="source-badge">Sample</span></td></tr>)}{!visible.length&&<tr><td colSpan={7} className="empty-state"><Search size={22}/><strong>No readings found</strong><span>Try a different search or time range.</span></td></tr>}</tbody></table></div><div className="table-footer"><span aria-live="polite">{filtered.length?`Showing ${page*6+1}–${Math.min((page+1)*6,filtered.length)} of ${filtered.length} readings`:'0 matching readings'}</span><div className="pagination"><button aria-label="Previous page" disabled={page===0} onClick={() => setPage(p => p-1)}><ChevronLeft size={15}/></button><span>Page {page+1} of {pages}</span><button aria-label="Next page" disabled={page+1>=pages} onClick={() => setPage(p => p+1)}><ChevronRight size={15}/></button></div></div></section>
    <div className="project-description">Development of a LoRa-Based Geolocation and Weather Assisting System for Small-Scale Fishermen in Barangay Acmac, Iligan City, Lanao del Norte</div>
    <footer className="page-footer"><span><Radio size={14}/> Acmac fisheries research <span className="footer-dot">·</span> LoRa geolocation &amp; weather assistance</span><span>Sample data only <span className="footer-dot">·</span> PHT (UTC+8)</span></footer>
   </main>
  </div>
  {help && <InfoDialog onClose={() => setHelp(false)} />}
 </div>;
}
