import {useMemo,useRef,useState} from 'react';
import {ArrowDownToLine,Search,X} from 'lucide-react';
import type {Device,Reading,SosEvent} from './data';
import type {Row} from './studySf';
import {studyCsv} from './studyDashboardData';
import {hostedLogs,demoLogs,filterLogs,fullTime,EMPTY_FILTERS,type LogFilters,type LogEntry} from './logsModel';
import {NO_VIEWS,type ViewSnapshot} from './relationalViews';
import type {SnapshotPhase} from './refreshState';
import {PAGE_SIZE,RecordTableBody,TablePagination} from './RecordTable';
function download(csv:string,name:string){const url=URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8;'}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
function exportStudy(rows:Row[]){download(studyCsv(rows),'acmac-non-fixture-records.csv');}
function exportDemo(rows:LogEntry[]){const quote=(v:unknown)=>'"'+String(v??'').replace(/"/g,'""')+'"';download(['sample_capture_utc,node,vessel,type,observation,source',...rows.map(r=>[r.capture===null?'':new Date(r.capture).toISOString(),r.node,r.vessel,r.kind,r.description,'Sample'].map(quote).join(','))].join('\n'),'acmac-fictional-logs.csv');}
type Props={rows?:Row[];readings:Reading[];samples:SosEvent[];devices:Device[];phase?:SnapshotPhase;views?:ViewSnapshot};
export default function StudyHistory({rows,readings,samples,devices,phase='ready',views=NO_VIEWS}:Props){
 const hosted=rows!==undefined;
 const [filters,setFilters]=useState<LogFilters>(EMPTY_FILTERS),[page,setPage]=useState(0);
 const searchRef=useRef<HTMLInputElement>(null);
 const entries=useMemo(()=>rows===undefined?demoLogs(readings,samples,devices):hostedLogs(rows,devices,views),[rows,readings,samples,devices,views]);
 const pending=phase==='loading'||phase==='error';
 const matched=pending?[]:filterLogs(entries,filters),current=Math.min(page,Math.max(0,Math.ceil(matched.length/PAGE_SIZE)-1));
 const visible=matched.slice(current*PAGE_SIZE,current*PAGE_SIZE+PAGE_SIZE),active=Object.values(filters).some(Boolean);
 const update=(field:keyof LogFilters,val:string)=>{setFilters(old=>({...old,[field]:val}));setPage(0);};
 const clear=()=>{update('query','');searchRef.current?.focus();};
 const doExport=()=>hosted?exportStudy(matched.flatMap(r=>r.raw?[r.raw]:[])):exportDemo(matched);
 return <section className="panel readings-panel package-b-records" id="readings" aria-busy={phase==='loading'}>
  <div className="readings-heading"><div><h2>Logs <span className="number-badge">{pending?'—':entries.length}</span></h2><p>{hosted?'All synchronized non-fixture observations, newest known capture first.':'Fictional GPS, weather and SOS observations.'} · Philippine Standard Time (UTC+8).</p></div>
   <div className="table-actions"><div className="log-search"><Search size={16}/><input ref={searchRef} type="text" aria-label="Search logs" placeholder="Search logs…" value={filters.query} onChange={e=>update('query',e.target.value)}/><button type="button" className={'log-clear'+(!filters.query?' invisible':'')} disabled={!filters.query} aria-label="Clear search" onClick={clear}><X size={15}/></button></div><button type="button" className="icon-button table-export" aria-label="Export all matching logs" title="Export all matching logs (all pages)" disabled={pending||!matched.length} onClick={doExport}><ArrowDownToLine size={17}/></button></div>
  </div>
  <div className="log-filters"><label>Vessel<select value={filters.node} onChange={e=>update('node',e.target.value)}><option value="">All vessels</option>{devices.map(d=><option value={d.id} key={d.id}>{d.name}{d.registeredName?' · '+d.registeredName:''}</option>)}</select></label><label>Type<select value={filters.kind} onChange={e=>update('kind',e.target.value)}><option value="">All types</option><option value="gps">GPS</option><option value="weather">Weather</option><option value="sos">SOS</option><option value="cancel">CANCEL</option></select></label><label>Capture from (PHT)<input type="date" value={filters.from} max={filters.to||undefined} onChange={e=>update('from',e.target.value)}/></label><label>Capture through (PHT)<input type="date" value={filters.to} min={filters.from||undefined} onChange={e=>update('to',e.target.value)}/></label><button className="button" type="button" disabled={!active} onClick={()=>{setFilters(EMPTY_FILTERS);setPage(0)}}>Reset filters</button></div>
  <p className="log-help">Search vessel name, node ID, registration number, record ID, type, observation, source or time. Date filters exclude unknown capture times. Export includes every matching page.</p>
  <div className="table-scroll record-table-scroll"><table className="record-table"><colgroup>{[19,18,9,29,10,15].map((w,i)=><col key={i} style={{width:`${w}%`}}/>)}</colgroup><thead><tr><th>Capture time (PHT)</th><th>Vessel / recorder</th><th>Type</th><th>Observation</th><th>Source</th><th>Backend receipt (PHT)</th></tr></thead>
   <RecordTableBody columns={6} count={visible.length} phase={phase} empty={!entries.length} filtered={active}>{visible.map(r=><tr className="record-data-row" key={r.key}><td>{fullTime(r.capture)}</td><td><div className="record-cell-text" title={r.vessel}>{r.vessel}</div>{r.registration&&<small title={r.registration}>{r.registration}</small>}</td><td>{r.kind.toUpperCase()}</td><td><div className="record-cell-text" title={r.description}>{r.description}</div></td><td>{r.source}</td><td>{hosted?fullTime(r.receipt):'Not applicable'}</td></tr>)}</RecordTableBody>
  </table></div>
  <TablePagination page={current} count={matched.length} onPage={setPage} disabled={pending}/>
  {hosted&&!pending&&<p className="log-help">{views.status==='ready'?`${entries.filter(r=>r.projected).length} of ${entries.length} records matched the relational projections; all authorized non-fixture raw observations are retained.`:views.status==='unavailable'?'Relational projections unavailable; displaying authorized raw DTN records.':'Raw DTN records · local historical review.'}</p>}
 </section>;
}
