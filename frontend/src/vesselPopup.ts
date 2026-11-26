import type { Row } from './studySf.ts';
import type { Reading, SosEvent } from './data.ts';

export type PopupEmergency = { status: 'Needs attention' | 'Cancellation observed' | 'No SOS recorded'; observedAt: number | null };

// An SOS observation may have been recorded by a different vessel. Use origin,
// and never substitute backend receipt time for an unknown capture time.
export function popupEmergency(id: string, now: number, records: Row[] | undefined, samples: SosEvent[]): PopupEmergency {
 if (records === undefined) {
  const events = samples.filter(e=>e.deviceId===id && Number.isFinite(e.timestamp) && e.timestamp<=now);
  return events.length ? {status:'Needs attention',observedAt:Math.max(...events.map(e=>e.timestamp))} : {status:'No SOS recorded',observedAt:null};
 }
 const groups = new Map<number,{sos:boolean;cancel:boolean;times:number[]}>();
 for(const row of records){
  const {kind,payload,captured_at}=row.record_json;
  if((kind!=='sos'&&kind!=='cancel')||String(payload.origin_node_id)!==id||typeof payload.event_id!=='number'||!Number.isInteger(payload.event_id))continue;
  const group=groups.get(payload.event_id)??{sos:false,cancel:false,times:[]};
  if(kind==='cancel')group.cancel=true;
  else {group.sos=true;const t=captured_at===null?NaN:Date.parse(captured_at);if(Number.isFinite(t)&&t<=now)group.times.push(t);}
  groups.set(payload.event_id,group);
 }
 const open=[...groups.values()].filter(g=>g.sos&&!g.cancel);
 if(open.length){
  // Earliest known observation within each event, most recent such event across
  // open groups. Unknown-time events are not assigned a fabricated time.
  const times=open.filter(g=>g.times.length).map(g=>Math.min(...g.times));
  return {status:'Needs attention',observedAt:times.length?Math.max(...times):null};
 }
 return {status:[...groups.values()].some(g=>g.cancel)?'Cancellation observed':'No SOS recorded',observedAt:null};
}

export function popupBattery(reading: Reading | null | undefined): string {
 const value=reading?.battery;
 return typeof value==='number'&&Number.isFinite(value)&&value>=0&&value<=100?`${value} %`:'Unavailable';
}
