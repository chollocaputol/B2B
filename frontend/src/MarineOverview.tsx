import { Anchor, BatteryMedium, CloudSun, Compass, Droplets, Gauge, MapPin, Radio, ShieldAlert, Thermometer } from 'lucide-react';
import { ageLabel, dateTime, getVesselSnapshot, weatherLabel, type Device, type Reading, type SosEvent } from './data';
import type {studyDashboard} from './studyDashboardData';
import VesselMap, {type MapSession} from './VesselMap';
import type {SnapshotPhase} from './refreshState';

type Props = {devices:Device[];readings:Reading[];sosEvents:SosEvent[];selectedId:string;now:number;onSelect:(id:string)=>void;study?:ReturnType<typeof studyDashboard>;mapSession?:MapSession;studyPhase?:SnapshotPhase};
export default function MarineOverview(props:Props) {
 const {devices,readings,sosEvents,selectedId,now,onSelect}=props;
 const vessel = devices.find(d=>d.id===selectedId);
 const loading=!!props.study&&props.studyPhase==='loading';
 const failed=!!props.study&&props.studyPhase==='error';
 const snapshot = getVesselSnapshot(props.study?.positions??readings,selectedId,now);
 const {position,shoreDistance}=snapshot; const latest=props.study?props.study.latestWeather[selectedId]:snapshot.latest;
 const weather = latest?.weatherState ?? -1;
 const hasWeather = latest?.temperature !== null && latest?.temperature !== undefined;
 return <div className="marine-grid">
  <VesselMap {...props} readings={props.study?.positions??readings} synchronized={!!props.study}/>
  <div className="marine-side">
   <section className="panel weather-panel" id="weather"><div className="panel-heading"><div><h2><CloudSun size={17}/> Weather at the vessel</h2><p>{loading?'Loading vessel…':failed?'Vessel unavailable':vessel?.name??'No vessel selected'} · {props.study?'last timed synchronized measurement':'local sensor sample'}</p></div><span className="weather-icon"><CloudSun size={30}/></span></div>
    <div className="weather-main"><div><strong>{loading?<span className="value-skeleton" aria-label="Loading temperature"/>:hasWeather?latest!.temperature!.toFixed(1):'—'}<span>°C</span></strong><p>{loading?'Loading weather…':failed?'Unable to load weather':!vessel?'No vessel selected':hasWeather?'Air temperature':'Weather sensors unavailable'}</p></div><span className={`weather-state state-${weather}`}>{loading?'Loading…':failed||!vessel?'Unavailable':weatherLabel(weather)}</span></div>
    <div className="weather-details"><div><Droplets size={15}/><span>Humidity</span><strong>{loading?'Loading…':latest?.humidity!=null?`${latest.humidity.toFixed(1)} %`:'Unavailable'}</strong></div><div><Gauge size={15}/><span>Pressure</span><strong>{loading?'Loading…':latest?.pressure!=null?`${latest.pressure.toFixed(1)} hPa`:'Unavailable'}</strong></div><div><Thermometer size={15}/><span>Pressure trend</span><strong>{loading?'Loading…':latest?.pressureTrend!=null?`${latest.pressureTrend>0?'+':''}${latest.pressureTrend.toFixed(1)} hPa`:'Unavailable'}</strong></div></div>
    <div className="weather-note">{latest?`${props.study?'Captured':'Last packet'} ${ageLabel(latest.timestamp,now)} · `:''}Sensor state only; not an official forecast.</div>
   </section>
   <section className="panel sos-panel" id="alerts"><div className="panel-heading"><div><h2><ShieldAlert size={17}/> SOS activity <span className="scenario-badge">{props.study?'SYNCED':'DEMO'}</span></h2><p>{props.study?'Recorded emergency observations (all recording vessels)':'Read-only sample events'}</p></div></div>
    {props.study&&props.study.events.length===0&&<p className="sos-footnote" role="status">{loading?'Loading emergency observations…':failed?'Unable to load emergency observations.':'No synchronized emergency observations in this dataset.'}</p>}
    {props.study?props.study.events.map(e=><div className="sos-event" key={`${e.origin}/${e.event}`}><strong>Origin {e.origin}, event {e.event}</strong><p>{e.state} · SOS {e.sos}, CANCEL {e.cancel}</p></div>):sosEvents.map(event=><div className="sos-event" key={`${event.deviceId}-${event.timestamp}`}><div className="sos-event-title"><span className="sos-dot"/><strong>Sample SOS received</strong><span>{ageLabel(event.timestamp,now)}</span></div><p>{devices.find(d=>d.id===event.deviceId)?.name} · {dateTime(event.timestamp)} PHT</p><div className="sos-coordinates"><MapPin size={13}/>{event.latitude.toFixed(5)}° N, {event.longitude.toFixed(5)}° E</div><small>Coordinates at the SOS event; current position may differ.</small><button className="sos-select" onClick={()=>onSelect(event.deviceId)}>View vessel details <Compass size={14}/></button></div>)}
    <p className="sos-footnote">{props.study?'Cancellation observed is not proof of rescue completion or radio source authentication. No rescue request is sent.':'Demonstration only. No distress message or rescue request is being sent.'}</p>
   </section>
  </div>
  <section className="panel vessel-detail-strip" aria-label="Selected vessel details"><div><Anchor size={18}/><span>Selected vessel<strong>{loading?'Loading…':failed?'Unavailable':vessel?.name??'—'}</strong></span></div><div><MapPin size={17}/><span>Shore distance at last fix<strong title={position?`${ageLabel(position.timestamp,now)} · ${dateTime(position.timestamp)} PHT`:undefined}>{loading?'Loading…':shoreDistance!=null?`${(shoreDistance/1000).toFixed(1)} km`:'Unavailable'}</strong></span></div><div><BatteryMedium size={18}/><span>Battery sample<strong>{loading?'Loading…':latest?.battery!=null?`${latest.battery} %`:'Unavailable'}</strong></span></div><div><Radio size={18}/><span>Received LoRa RSSI<strong>{loading?'Loading…':latest?.signal!=null?`${latest.signal} dBm`:'Unavailable'}</strong></span></div></section>
 </div>;
}
