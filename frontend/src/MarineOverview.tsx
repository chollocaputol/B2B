import type { MouseEvent } from 'react';
import EmergencyHistory from './EmergencyHistory';
import WeatherLayers from './WeatherLayers';
import {batteryLabel,batteryAge} from './wxbatPresentation';
import type {ViewSnapshot} from './relationalViews';
import { Anchor, ArrowRight, BatteryMedium, CloudSun, Compass, Droplets, Gauge, MapPin, ShieldAlert, Thermometer } from 'lucide-react';
import { ageLabel, dateTime, getVesselSnapshot, weatherLabel, type Device, type Reading, type SosEvent } from './data';
import type {studyDashboard} from './studyDashboardData';
import VesselMap, {type MapSession} from './VesselMap';
import type {SnapshotPhase} from './refreshState';
import {sectionPath,type Section} from './routes';
import {emergencyReports,summarizeEmergencyReports,reportTime} from './vesselPopup';
import {distanceToShore,distanceText,vesselContext} from './geolocation';

type Props = {views?:ViewSnapshot;devices:Device[];readings:Reading[];sosEvents:SosEvent[];selectedId:string;now:number;onSelect:(id:string)=>void;study?:ReturnType<typeof studyDashboard>;mapSession?:MapSession;tilesEnabled:boolean;studyPhase?:SnapshotPhase;section?:Exclude<Section,'records'>;onViewSos:(event:MouseEvent<HTMLAnchorElement>)=>void};
function OverviewSos(props:Props) {
 const {study,studyPhase,now,sosEvents,devices,onViewSos}=props;
 const pending=!!study&&studyPhase==='loading',failed=!!study&&studyPhase==='error';
 const reports=pending||failed?[]:emergencyReports(now,study?study.records:undefined,sosEvents);
 const {attentionCount,unknownTimeCount,report}=summarizeEmergencyReports(reports);
 const time=report?reportTime(report):null;
 const origin=report?(devices.find(d=>d.id===report.originId)?.name??(study?'Vessel '+report.originId:report.originId)):'';
 return <section className="panel sos-panel sos-overview" id="alerts" aria-busy={pending}>
  <div className="panel-heading"><div><h2><ShieldAlert size={17}/> SOS activity <span className="scenario-badge">{study?'SYNCED':'DEMO'}</span></h2><p>{pending?'Loading emergency observations…':failed?'Emergency observations unavailable':attentionCount?attentionCount+' '+(attentionCount===1?'report needs':'reports need')+' attention':'No recorded SOS awaiting cancellation'}</p></div></div>
  {pending||failed?<p className="sos-footnote" role="status">{pending?'Refreshing emergency observations…':'Unable to load emergency observations.'}</p>:report?<div className={'sos-event sos-summary-event'+(report.status==='Needs attention'?' needs-attention':'')}>
   <div className="sos-summary-status"><span className="sos-summary-dot"/>{report.status}</div>
   <strong>{origin}</strong>{report.eventId!==null&&<small>Event {report.eventId}</small>}
   <p>{report.sosCount?'SOS observed at':'Cancellation observed at'}: {time===null?'Unknown':dateTime(time)+' PHT'}</p>
   <small>{report.status==='Needs attention'?'No matching cancellation recorded.':report.status==='Cancellation only'?'Original SOS observation unavailable.':'Matching cancellation recorded; rescue outcome unknown.'}</small>
   {unknownTimeCount>0&&<small>{unknownTimeCount} {unknownTimeCount===1?'relevant report has':'relevant reports have'} unknown observation time; chronological order may be incomplete.</small>}
  </div>:<p className="sos-footnote" role="status">{study?'No synchronized emergency observations in this dataset.':'No sample emergency observations.'}</p>}
  <p className="sos-footnote">{study?'Map markers indicate last GPS fixes, not distress locations. Reports without a valid fix may have no marker. No rescue request is sent.':'Fictional observations only. No distress message or rescue request is being sent.'}</p>
  <a className="panel-link sos-summary-link" href={sectionPath('sos',!study)} onClick={onViewSos}>View SOS activity <ArrowRight size={15}/></a>
 </section>;
}

export default function MarineOverview(props:Props) {
 const {devices,readings,sosEvents,selectedId,now,onSelect}=props;
 const vessel = devices.find(d=>d.id===selectedId);
 const loading=!!props.study&&props.studyPhase==='loading';
 const failed=!!props.study&&props.studyPhase==='error';
 const snapshot = getVesselSnapshot(props.study?.positions??readings,selectedId,now);
 const {position}=snapshot;const shoreDistance=distanceToShore(position,!!props.study);
 const context=props.study&&vessel?vesselContext(props.study.records,vessel.id,now):null;
 const stamp=(t:number|null|undefined)=>t==null?'Unavailable':dateTime(t)+' PHT'; const latest=props.study?props.study.latestWeather[selectedId]:snapshot.latest;
 const battery=props.study?.latestBattery[selectedId];
 const weather = latest?.weatherState ?? -1;
 const hasWeather = latest?.temperature !== null && latest?.temperature !== undefined;
 const section=props.section??'overview';
 const overview=section==='overview';
 return <div className={overview?'marine-grid overview-map-first':'marine-grid section-single'}>
  {(overview||section==='vessels')&&<VesselMap {...props} readings={props.study?.positions??readings} synchronized={!!props.study}/>}
  {(overview||section==='weather'||section==='sos')&&<div className="marine-side">
   {overview&&<section className="panel weather-panel" id="weather"><div className="panel-heading"><div><h2><CloudSun size={17}/> Weather at the vessel</h2><p>{loading?'Loading vessel…':failed?'Vessel unavailable':vessel?.name??'No vessel selected'} · {props.study?'last timed synchronized measurement':'local sensor sample'}</p></div><span className="weather-icon"><CloudSun size={30}/></span></div>
    <div className="weather-main"><div><strong>{loading?<span className="value-skeleton" aria-label="Loading temperature"/>:hasWeather?latest!.temperature!.toFixed(1):'—'}<span>°C</span></strong><p>{loading?'Loading weather…':failed?'Unable to load weather':!vessel?'No vessel selected':hasWeather?'Air temperature':'Weather sensors unavailable'}</p></div><span className={`weather-state state-${weather}`}>{loading?'Loading…':failed||!vessel?'Unavailable':weatherLabel(weather)}</span></div>
    <div className="weather-details"><div><Droplets size={15}/><span>Humidity</span><strong>{loading?'Loading…':latest?.humidity!=null?`${latest.humidity.toFixed(1)} %`:'Unavailable'}</strong></div><div><Gauge size={15}/><span>Pressure</span><strong>{loading?'Loading…':latest?.pressure!=null?`${latest.pressure.toFixed(1)} hPa`:'Unavailable'}</strong></div><div><Thermometer size={15}/><span>Pressure trend</span><strong>{loading?'Loading…':latest?.pressureTrend!=null?`${latest.pressureTrend>0?'+':''}${latest.pressureTrend.toFixed(1)} hPa`:'Unavailable'}</strong></div></div>
    <div className="weather-note">{latest?`${props.study?'Captured':'Last packet'} ${ageLabel(latest.timestamp,now)} · `:''}Sensor state only; not an official forecast.</div>
   </section>}
   {overview&&<OverviewSos {...props}/>}
   {(overview||section==='weather')&&<WeatherLayers rows={props.study?.observations} nodeId={selectedId} now={now} phase={props.studyPhase} compact={overview}/>}
   {section==='sos'&&<EmergencyHistory rows={props.study?.records} samples={sosEvents} devices={devices} now={now} phase={props.studyPhase} views={props.views}/>}
  </div>}
  {(overview||section==='vessels')&&<section className="panel vessel-detail-strip" aria-label="Selected vessel details"><div><Anchor size={18}/><span>Selected vessel<strong>{loading?'Loading…':failed?'Unavailable':vessel?.name??'—'}</strong></span></div><div><MapPin size={17}/><span>Shore distance at last fix<strong title={position?`${ageLabel(position.timestamp,now)} · ${dateTime(position.timestamp)} PHT`:undefined}>{loading?'Loading…':distanceText(shoreDistance)}</strong></span></div><div><BatteryMedium size={18}/><span>{props.study?'Battery sample · recorded estimate':'Battery sample'}<strong title={props.study&&battery?.captureAt!=null?stamp(battery.captureAt):undefined}>{loading?'Loading…':failed?'Unavailable':props.study?batteryLabel(battery):latest?.battery!=null?`${latest.battery} %`:'Unavailable'}</strong>{props.study&&!loading&&!failed&&<small>{batteryAge(battery,now)}</small>}</span></div></section>}
  {section==='vessels'&&<section className="panel vessel-context-panel" aria-label="Vessel observation details">
   <div className="panel-heading"><div><h2>Vessel information</h2><p>{props.study?'Registry and recorded observation context':'Fictional sample context'}</p></div></div>
   {loading||failed?<p className="data-state-note" role="status">{loading?'Loading vessel information…':'Vessel information unavailable.'}</p>:!vessel?<p className="data-state-note">No vessel selected.</p>:<>
    {props.study?.registryStatus!=='ready'&&props.study&&<p className="data-state-note" role="status">{props.study.registryStatus==='unavailable'?'Registry could not be retrieved. Observation history remains available.':'Registry is not configured in this local review.'}</p>}
    <dl className="vessel-context-grid">
     <div><dt>Node ID</dt><dd>{vessel.id}</dd></div><div><dt>Vessel name</dt><dd>{vessel.registeredName??(props.study?'Unavailable':vessel.name)}</dd></div><div><dt>Registration no.</dt><dd>{vessel.registrationSerialNo??'Unavailable'}</dd></div>
     <div><dt>Last valid GPS fix</dt><dd>{stamp(position?.timestamp)}</dd></div><div><dt>Last valid coordinates</dt><dd>{position?position.latitude!.toFixed(5)+', '+position.longitude!.toFixed(5):'Unavailable'}</dd></div><div><dt>Shore reference distance</dt><dd>{distanceText(shoreDistance)}</dd></div>
     {props.study&&<><div><dt>Latest timed GPS attempt</dt><dd>{stamp(context?.attemptAt)}{context?.attemptFix!=null?' · '+(context.attemptFix?'Fix reported':'No fix'):''}</dd></div><div><dt>Latest timed capture</dt><dd>{stamp(context?.captureAt)}</dd></div><div><dt>Latest backend receipt</dt><dd>{stamp(context?.receiptAt)}</dd></div></>}
    </dl>
    <p className="data-state-note">{props.study?'Recorded observations only. Receipt is not capture time; unknown or ambiguous capture times are not inferred.':'Sample values only.'}</p>
   </>}
  </section>}
 </div>;
}
