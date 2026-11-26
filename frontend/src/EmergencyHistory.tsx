import {useState} from 'react';
import {ShieldAlert} from 'lucide-react';
import type {Device,SosEvent} from './data';
import type {Row} from './studySf';
import type {SnapshotPhase} from './refreshState';
import {groupedSos,fullTime} from './logsModel';
import {NO_VIEWS,type ViewSnapshot} from './relationalViews';
import {RecordTableBody,TablePagination} from './StudyHistory';
export default function EmergencyHistory({rows,samples,devices,now,phase='ready',views=NO_VIEWS}:{rows?:Row[];samples:SosEvent[];devices:Device[];now:number;phase?:SnapshotPhase;views?:ViewSnapshot}){
 const [status,setStatus]=useState(''),[page,setPage]=useState(0);
 const hosted=rows!==undefined,pending=phase==='loading'||phase==='error';
 const reports=pending?[]:groupedSos(now,rows,samples,devices,views);
 const attention=reports.filter(r=>r.status==='Needs attention').length,matched=reports.filter(r=>!status||r.status===status);
 const current=Math.min(page,Math.max(0,Math.ceil(matched.length/6)-1)),visible=matched.slice(current*6,current*6+6);
 return <section className="panel readings-panel package-b-records" id="alerts" aria-busy={phase==='loading'}>
  <div className="readings-heading"><div><h2><ShieldAlert size={17}/> SOS activity <span className="scenario-badge">{hosted?'SYNCED':'DEMO'}</span></h2><p>One event per originating vessel and event ID · delayed observations, not live distress tracking.</p></div><span className={attention?'emergency-count active':'emergency-count'}>{pending?'—':`${attention} ${attention===1?'event needs':'events need'} attention`}</span></div>
  <div className="log-filters"><label>Status<select value={status} onChange={e=>{setStatus(e.target.value);setPage(0)}}><option value="">All events</option><option>Needs attention</option><option>Cancellation observed</option><option>Cancellation only</option></select></label></div>
  <div className="table-scroll record-table-scroll"><table className="record-table emergency-table"><colgroup>{[18,22,9,19,18,14].map((w,i)=><col key={i} style={{width:`${w}%`}}/>)}</colgroup><thead><tr><th>Status</th><th>Originating vessel</th><th>Event</th><th>SOS observed (PHT)</th><th>Cancellation observed (PHT)</th><th>Last receipt (PHT)</th></tr></thead>
   <RecordTableBody columns={6} count={visible.length} phase={phase} empty={!reports.length} filtered={!!status}>{visible.map(r=><tr key={r.key} className="record-data-row"><td><span className={'emergency-status'+(r.status==='Needs attention'?' active':'')}>{r.status}</span></td><td><div className="record-cell-text" title={r.vessel}>{r.vessel}</div>{hosted&&<small>{r.recordingNodes} recording {r.recordingNodes===1?'node':'nodes'}</small>}</td><td>{r.eventId??'Sample'}</td><td>{r.sosCount?fullTime(r.observedAt):'Not recorded'}</td><td>{r.cancelCount?fullTime(r.cancellationObservedAt):'Not recorded'}</td><td>{hosted?fullTime(r.lastReceipt):'Not applicable'}</td></tr>)}</RecordTableBody>
  </table></div>
  <TablePagination page={current} count={matched.length} onPage={setPage} disabled={pending}/>
  <p className="log-help">Only a matching recorded cancellation clears attention. No time-based expiry. Cancellation does not establish rescue completion. No rescue request is sent.</p>
  {hosted&&views.status==='unavailable'&&!pending&&<p className="log-help">Relational projections unavailable; events are grouped from authorized raw records.</p>}
 </section>;
}
