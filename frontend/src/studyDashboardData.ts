import { latestTimed, projectStudy, type Row } from './studySf.ts';
import type { Device, Reading } from './data.ts';
import {registryDevices,type RegistryRow,type RegistryState} from './geolocation.ts';
import {recordedBattery,batteryPercent,macroObservation,macroCandidate,macroResultLabel,macroIndexLabel,type RecordedBattery} from './wxbatPresentation.ts';
import {indexViews,matchedProjection,NO_VIEWS,type ViewSnapshot} from './relationalViews.ts';
const numberOrNull = (x: unknown): number | null => typeof x === 'number' && Number.isFinite(x) ? x : null;
const capture = (r:Row) => r.record_json.captured_at === null ? NaN : Date.parse(r.record_json.captured_at);
function reading(r:Row): Reading {
 const p=r.record_json.payload, weather=r.record_json.kind==='weather';
 return {deviceId:String(r.recorder_node_id),timestamp:capture(r),temperature:weather?numberOrNull(p.temperature_c):null,humidity:weather?numberOrNull(p.humidity_percent):null,pressure:weather?numberOrNull(p.pressure_hpa):null,pressureTrend:weather?numberOrNull(p.pressure_trend_hpa):null,weatherState:weather?(numberOrNull(p.weather_state)??-1):-1,latitude:weather?null:numberOrNull(p.latitude),longitude:weather?null:numberOrNull(p.longitude),gpsFix:!weather&&p.gps_fix===true,signal:null,battery:null,shoreDistance:null};
}
export function studyDashboard(rows:Row[], now:number, registry:RegistryRow[]=[], registryStatus:RegistryState='not-configured',views:ViewSnapshot=NO_VIEWS) {
 const p=projectStudy(rows);
 const indexed=indexViews(views);
 // Only consistent projections may enrich display; raw records remain authoritative.
 const observations=p.records.map(r=>matchedProjection(r,indexed)??r);
 const devices:Device[]=registryDevices(p.nodes.map(n=>({id:String(n),name:`Vessel ${n}`,location:'Recording vessel · synchronized history'})),registry);
 const readings:Reading[]=[], latestWeather:Record<string,Reading|undefined>={}, positions:Reading[]=[], latestBattery:Record<string,RecordedBattery|undefined>={};
 for(const d of devices){
  latestBattery[d.id]=recordedBattery(observations,d.id,now);
  const weather=observations.filter(r=>r.record_json.kind==='weather'&&String(r.recorder_node_id)===d.id&&capture(r)<=now);
  const grouped=new Map<number,Row[]>();for(const r of weather){const t=capture(r);grouped.set(t,[...(grouped.get(t)??[]),r]);}
  for(const group of grouped.values()){const r=latestTimed(group);if(r)readings.push(reading(r));else readings.push({...reading(group[0]),temperature:null,humidity:null,pressure:null,pressureTrend:null,weatherState:-1});}
  const w=latestTimed(weather);if(w)latestWeather[d.id]=reading(w);
  const g=latestTimed(observations.filter(r=>r.record_json.kind==='gps').filter(r=>{const q=r.record_json.payload,lat=numberOrNull(q.latitude),lon=numberOrNull(q.longitude);return String(r.recorder_node_id)===d.id&&capture(r)<=now&&q.gps_fix===true&&lat!==null&&lon!==null&&Math.abs(lat)<=90&&Math.abs(lon)<=180;}));if(g)positions.push(reading(g));
 }
 return {...p,observations,devices,readings,latestWeather,latestBattery,positions,sosEvents:[],registryStatus};
}
export function describeRecord(r:Row):string {
 const p=r.record_json.payload, val=(x:unknown)=>numberOrNull(x)??'Unavailable';
 const micro=p.micro as {input_valid?:boolean;state?:string;pattern?:string}|null|undefined;
 const microLabel=micro?.input_valid===true&&typeof micro.state==='string'&&typeof micro.pattern==='string'?`${micro.state} / ${micro.pattern}`:'Unavailable';
 const macro=macroObservation(p),weatherOutput=macroCandidate(p)?`macro ${macroResultLabel(macro)}; index ${macroIndexLabel(macro)}`:`micro ${microLabel}`;
 const battery=batteryPercent(p.battery_percent),batterySuffix=Object.hasOwn(p,'battery_percent')?`; battery ${battery===null?'Unavailable':battery+' % (recorded estimate)'}`:'';
 switch(r.record_json.kind){case 'weather':return `${val(p.temperature_c)} °C; ${val(p.humidity_percent)} %; ${val(p.pressure_hpa)} hPa; trend ${val(p.pressure_trend_hpa)} hPa; ${weatherOutput}`;case 'gps':return (p.gps_fix===true?`GPS fix: ${val(p.latitude)}, ${val(p.longitude)}`:'No GPS fix')+batterySuffix;default:return `${r.record_json.kind.toUpperCase()} · origin ${val(p.origin_node_id)}, event ${val(p.event_id)} · ${p.source==='local'?'Local':'Received'}`;}
}
export function studyCsv(rows:Row[]):string {
 const quote=(v:unknown)=>'"'+String(v??'').replace(/"/g,'""')+'"';
 return ['recorder_node_id,record_id,kind,captured_at_utc,synced_at_utc,uptime_ms,record_json',...rows.map(r=>[r.recorder_node_id,r.record_id,r.record_json.kind,r.record_json.captured_at,r.synced_at,r.record_json.uptime_ms,JSON.stringify(r.record_json)].map(quote).join(','))].join('\n');
}

// Exact fixture identities from the six accepted request fixtures (local lab only).
// This is exclusion of known fixtures, not authentication of every other record.
const knownFixtureIds = new Set([
 ...Array.from({length:40},(_,i)=>`3/aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:${i+1}`),
 ...Array.from({length:8},(_,i)=>`3/bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb:${i+1}`),
]);
export function excludeKnownFixtures(rows:Row[]) {
 const verified=projectStudy(rows).records;
 const records=verified.filter(r=>!knownFixtureIds.has(`${r.recorder_node_id}/${r.record_id}`));
 return {records,excluded:verified.length-records.length};
}
// Newest known capture first; unknown times last. Receipt time is only a
// secondary display ordering, never an invented capture time.
export function orderStudyHistory(rows:Row[]):Row[] {
 return [...rows].sort((a,b)=>{
  const ta=capture(a),tb=capture(b),ka=Number.isFinite(ta),kb=Number.isFinite(tb);
  if(ka!==kb)return ka?-1:1;
  if(ka&&kb&&ta!==tb)return tb-ta;
  const ra=Date.parse(a.synced_at),rb=Date.parse(b.synced_at);
  if(Number.isFinite(ra)&&Number.isFinite(rb)&&ra!==rb)return rb-ra;
  return a.recorder_node_id-b.recorder_node_id||a.record_id.localeCompare(b.record_id);
 });
}
