import type { Row } from './studySf.ts';
import type { Reading, SosEvent } from './data.ts';

export type PopupEmergency = { status: 'Needs attention' | 'Cancellation observed' | 'No SOS recorded'; observedAt: number | null };
export type EmergencyReport = {
 key:string; originId:string; eventId:number|null; sosCount:number; cancelCount:number;
 observedAt:number|null; cancellationObservedAt:number|null;
 status:'Needs attention'|'Cancellation observed'|'Cancellation only';
};
const knownTime=(t:number,now:number)=>Number.isFinite(t)&&t<=now?t:null;
const earlier=(a:number|null,b:number|null)=>a===null?b:b===null?a:Math.min(a,b);

// Match by originating node and event, never by recording vessel. A cancellation
// dominates its own event even when the capture time is unknown or reordered.
export function emergencyReports(now:number,records:Row[]|undefined,samples:SosEvent[]):EmergencyReport[] {
 const groups=new Map<string,EmergencyReport>();
 if(records===undefined){
  for(const e of samples){
   if(knownTime(e.timestamp,now)===null)continue;
   const key=JSON.stringify([e.deviceId,e.timestamp]);
   groups.set(key,{key,originId:e.deviceId,eventId:null,sosCount:1,cancelCount:0,observedAt:e.timestamp,cancellationObservedAt:null,status:'Needs attention'});
  }
 }else{
  for(const row of records){
   const {kind,payload,captured_at}=row.record_json;
   const originId=String(payload.origin_node_id),eventId=payload.event_id;
   if((kind!=='sos'&&kind!=='cancel')||!/^\d+$/.test(originId)||typeof eventId!=='number'||!Number.isInteger(eventId))continue;
   const key=JSON.stringify([originId,eventId]);
   const group:EmergencyReport=groups.get(key)??{key,originId,eventId,sosCount:0,cancelCount:0,observedAt:null,cancellationObservedAt:null,status:'Needs attention'};
   const time=knownTime(captured_at===null?NaN:Date.parse(captured_at),now);
   if(kind==='sos'){group.sosCount++;group.observedAt=earlier(group.observedAt,time);}
   else{group.cancelCount++;group.cancellationObservedAt=earlier(group.cancellationObservedAt,time);}
   group.status=group.cancelCount?(group.sosCount?'Cancellation observed':'Cancellation only'):'Needs attention';
   groups.set(key,group);
  }
 }
 return [...groups.values()];
}

export function popupEmergencyFromReports(id:string,reports:EmergencyReport[]):PopupEmergency {
 const own=reports.filter(g=>g.originId===id);
 const open=own.filter(g=>g.status==='Needs attention');
 if(open.length){
  const observedAt=open.reduce<number|null>((latest,g)=>g.observedAt===null?latest:latest===null?g.observedAt:Math.max(latest,g.observedAt),null);
  return {status:'Needs attention',observedAt};
 }
 return {status:own.some(g=>g.cancelCount>0)?'Cancellation observed':'No SOS recorded',observedAt:null};
}
export function popupEmergency(id: string, now: number, records: Row[] | undefined, samples: SosEvent[]): PopupEmergency {
 return popupEmergencyFromReports(id,emergencyReports(now,records,samples));
}

// Known SOS observation times take precedence; cancellation-only reports use
// their own known cancellation observation time, never backend receipt time.
export const reportTime=(g:EmergencyReport)=>g.sosCount?g.observedAt:g.cancellationObservedAt;
export function summarizeEmergencyReports(reports:EmergencyReport[]) {
 const open=reports.filter(g=>g.status==='Needs attention');
 const relevant=open.length?open:reports;
 const ordered=[...relevant].sort((a,b)=>{
  const ta=reportTime(a),tb=reportTime(b);
  if(ta===null&&tb!==null)return 1;
  if(ta!==null&&tb===null)return -1;
  if(ta!==null&&tb!==null&&ta!==tb)return tb-ta;
  return a.key.localeCompare(b.key);
 });
 return {attentionCount:open.length,unknownTimeCount:relevant.filter(g=>reportTime(g)===null).length,report:ordered[0]??null};
}

export function popupBattery(reading: Reading | null | undefined): string {
 const value=reading?.battery;
 return typeof value==='number'&&Number.isFinite(value)&&value>=0&&value<=100?`${value} %`:'Unavailable';
}
