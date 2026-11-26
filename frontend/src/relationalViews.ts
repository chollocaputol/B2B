import type { SupabaseClient } from '@supabase/supabase-js';
import {isAccessFailure} from './refreshState.ts';
import type {Row} from './studySf.ts';
export type ViewRow=Record<string,unknown>;
const core='recorder_node_id,record_id,captured_at,synced_at';
const fields={
 synchronization_history:core+',kind',
 vessel_telemetry:core+',gps_fix,latitude,longitude',
 weather_observations:core+',temperature_c,humidity_percent,pressure_hpa,pressure_trend_hpa,weather_state,micro_input_valid,micro_state,micro_pattern',
 sos_observations:core+',kind,origin_node_id,event_id,source',
 sos_events:'origin_node_id,event_id,sos_observation_count,cancel_observation_count,recording_node_count,first_known_sos_capture_at,first_known_cancel_capture_at,first_receipt_at,last_receipt_at'
} as const;
export type ViewName=keyof typeof fields;
export type ViewSnapshot={status:'ready';rows:Record<ViewName,ViewRow[]>}|{status:'unavailable'|'not-configured'};
export const NO_VIEWS:ViewSnapshot={status:'not-configured'};
export class ViewAccessError extends Error {}
// Read only the existing security-invoker views. Never use a service key.
export async function readRelationalViews(client:SupabaseClient):Promise<ViewSnapshot>{
 const results=await Promise.all((Object.keys(fields) as ViewName[]).map(async name=>{
  try{
  const rows:ViewRow[]=[];
  for(let offset=0;offset<=10000;offset+=500){
   const q=client.from(name).select(fields[name]);
   const ordered=name==='sos_events'?q.order('origin_node_id').order('event_id'):q.order('recorder_node_id').order('record_id');
   const {data,error,status}=await ordered.range(offset,offset+499);
   if(isAccessFailure(status,error?.code))return {name,access:false,rows:null};
   if(error||!data)return {name,access:true,rows:null};
   rows.push(...(data as unknown as ViewRow[]));
   if(rows.length>10000)return {name,access:true,rows:null};
   if(data.length<500)return {name,access:true,rows};
  }
  return {name,access:true,rows:null};
  }catch(e){const failure=e as {status?:number;code?:string}|null;return {name,access:!isAccessFailure(failure?.status??0,failure?.code),rows:null};}
 }));
 if(results.some(r=>!r.access))throw new ViewAccessError('Operator relational view access could not be verified.');
 if(results.some(r=>!r.rows))return {status:'unavailable'};
 return {status:'ready',rows:Object.fromEntries(results.map(r=>[r.name,r.rows])) as Record<ViewName,ViewRow[]>};
}
export const timeValue=(x:unknown):number|null=>typeof x==='string'&&Number.isFinite(Date.parse(x))?Date.parse(x):null;
const sameTime=(a:unknown,b:unknown)=>a==null&&b==null||timeValue(a)!==null&&timeValue(a)===timeValue(b);
const same=(a:unknown,b:unknown)=>(a??null)===(b??null);
function keyed(rows:ViewRow[],key:(row:ViewRow)=>string){
 const map=new Map<string,ViewRow|null>();for(const row of rows){const k=key(row);map.set(k,map.has(k)?null:row);}return map;
}
const rowKey=(r:ViewRow)=>`${r.recorder_node_id}/${r.record_id}`;
const eventKey=(r:ViewRow)=>`${r.origin_node_id}/${r.event_id}`;
export function indexViews(snapshot:ViewSnapshot){
 const rows=snapshot.status==='ready'?snapshot.rows:null;
 return {history:keyed(rows?.synchronization_history??[],rowKey),gps:keyed(rows?.vessel_telemetry??[],rowKey),weather:keyed(rows?.weather_observations??[],rowKey),sos:keyed(rows?.sos_observations??[],rowKey),events:keyed(rows?.sos_events??[],eventKey)};
}
// Inner joins omit unregistered nodes; concurrent uploads may produce different
// snapshots. A view is used only when its record identity and fields agree with raw DTN.
export function matchedProjection(row:Row,index:ReturnType<typeof indexViews>):Row|null{
 const key=`${row.recorder_node_id}/${row.record_id}`,r=row.record_json,p=r.payload,h=index.history.get(key);
 if(!h||h.kind!==r.kind||!sameTime(h.captured_at,r.captured_at)||!sameTime(h.synced_at,row.synced_at))return null;
 const v=(r.kind==='gps'?index.gps:r.kind==='weather'?index.weather:index.sos).get(key);
 if(!v||!sameTime(v.captured_at,r.captured_at)||!sameTime(v.synced_at,row.synced_at))return null;
 const names=r.kind==='gps'?['gps_fix','latitude','longitude']:r.kind==='weather'?['temperature_c','humidity_percent','pressure_hpa','pressure_trend_hpa','weather_state']:['origin_node_id','event_id','source'];
 if(!names.every(n=>same(v[n],p[n])))return null;
 if((r.kind==='sos'||r.kind==='cancel')&&v.kind!==r.kind)return null;
 const micro=p.micro as Record<string,unknown>|null|undefined;
 if(r.kind==='weather'&&(!same(v.micro_input_valid,micro?.input_valid)||!same(v.micro_state,micro?.state)||!same(v.micro_pattern,micro?.pattern)))return null;
 const payload={...p,...Object.fromEntries(names.map(n=>[n,v[n]]))};
 // Keep the original JSON for export. This projection is display-only.
 return {...row,synced_at:h.synced_at as string,record_json:{...r,captured_at:h.captured_at as string|null,payload}};
}
export function matchedEvent(index:ReturnType<typeof indexViews>,origin:string,event:number|null,rows:Row[],sosCount:number,cancelCount:number){
 if(event===null)return null;
 const v=index.events.get(`${origin}/${event}`);
 if(!v||v.sos_observation_count!==sosCount||v.cancel_observation_count!==cancelCount||v.recording_node_count!==new Set(rows.map(r=>r.recorder_node_id)).size)return null;
 const receipts=rows.map(r=>timeValue(r.synced_at)).filter((t):t is number=>t!==null);
 if(!receipts.length||timeValue(v.last_receipt_at)!==Math.max(...receipts)||timeValue(v.first_receipt_at)!==Math.min(...receipts))return null;
 return {lastReceipt:timeValue(v.last_receipt_at),recordingNodes:v.recording_node_count as number};
}
