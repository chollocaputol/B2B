import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import type {SnapshotPhase} from './refreshState';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { studyAccessMode, externalBasemapEnabled } from './studyAccessMode';
import { LocateFixed, MapPin, Navigation } from 'lucide-react';
import { ageLabel, dateTime, getLastPosition, getLatestReading, weatherLabel, type Device, type Reading, type SosEvent } from './data';
import type { studyDashboard } from './studyDashboardData';
import { popupEmergency, popupBattery } from './vesselPopup';

// In-memory view state owned by the authenticated dashboard, never browser storage.
export type MapSession = { viewport?: {center:[number,number];zoom:number}; selectedId?:string; tilesEnabled?:boolean };

type OpenInfo = {id:string;layer:L.Tooltip;marker:L.Marker};
function dismissInfo(instance:L.Map|null, active:{current:OpenInfo|null}, restoreFocus=false) {
 const info=active.current;
 if(!info)return;
 active.current=null;
 if(instance?.hasLayer(info.layer))instance.removeLayer(info.layer);
 const element=info.marker.getElement();
 element?.setAttribute('aria-expanded','false');
 if(restoreFocus)element?.focus({preventScroll:true});
}
function popupContent(device:Device, position:Reading, weather:Reading|null|undefined, sos:ReturnType<typeof popupEmergency>, synchronized:boolean, onClose:()=>void) {
 const card=document.createElement('div');card.className='vessel-info-card';
 const heading=document.createElement('div');heading.className='vessel-info-heading';
 const title=document.createElement('strong');title.textContent=device.name;
 const close=document.createElement('button');close.type='button';close.className='vessel-info-close';close.textContent='×';close.setAttribute('aria-label','Close vessel information');close.addEventListener('click',onClose);
 heading.append(title,close);card.append(heading);
 const list=document.createElement('dl');
 const attention=sos.status==='Needs attention';
 const fields=[['Registration no.','Unavailable'],['SOS status',sos.status],['Weather state',weatherLabel(weather?.weatherState??-1)],['Battery',popupBattery(weather)],[attention?'SOS observed at':'Last GPS fix',attention?(sos.observedAt===null?'Unknown':dateTime(sos.observedAt)+' PHT'):dateTime(position.timestamp)+' PHT']];
 for(const [label,value] of fields){const term=document.createElement('dt');term.textContent=label;const detail=document.createElement('dd');detail.textContent=value;list.append(term,detail);}
 card.append(list);
 const note=document.createElement('small');note.textContent=synchronized?(attention?'Recorded SOS observation; marker is last GPS fix, not distress location.':'Synchronized observations; not live position.'):'Fictional sample observations.';card.append(note);
 L.DomEvent.disableClickPropagation(card);L.DomEvent.disableScrollPropagation(card);
 return {card,close};
}

type Props = { study?:ReturnType<typeof studyDashboard>; devices: Device[]; readings: Reading[]; sosEvents: SosEvent[]; selectedId: string; now: number; onSelect: (id: string) => void; synchronized?:boolean; mapSession?:MapSession; tilesEnabled:boolean; studyPhase?:SnapshotPhase };
export default function VesselMap({ study, devices, readings, sosEvents, selectedId, now, onSelect, synchronized=false, mapSession, tilesEnabled, studyPhase='ready' }: Props) {
 const container = useRef<HTMLDivElement>(null);
 const map = useRef<L.Map | null>(null);
 const markers = useRef<Record<string,L.Marker>>({});
 const activeInfo=useRef<OpenInfo|null>(null);
 const [tileError,setTileError] = useState(false);
 const mode = studyAccessMode(location.hostname,location.protocol,import.meta.env.VITE_SUPABASE_URL);
 const showTiles = externalBasemapEnabled(synchronized,mode,tilesEnabled);
 useLayoutEffect(() => {
  if (!container.current) return;
  const initialFix = getLastPosition(readings,selectedId,now);
  const center: L.LatLngExpression = mapSession?.viewport?.center ?? (initialFix ? [initialFix.latitude!,initialFix.longitude!] : [8.283,124.245]);
  const instance = L.map(container.current,{scrollWheelZoom:false,maxZoom:18}).setView(center,mapSession?.viewport?.zoom ?? 13);
  map.current = instance;
  const rememberView = () => {
   if (!mapSession) return;
   const c = instance.getCenter();
   mapSession.viewport = {center:[c.lat,c.lng],zoom:instance.getZoom()};
  };
  rememberView();
  instance.on('moveend',rememberView);
  const observer = new ResizeObserver(() => instance.invalidateSize({animate:false,pan:true}));
  observer.observe(container.current);
  return () => { dismissInfo(instance,activeInfo); observer.disconnect(); instance.off('moveend',rememberView); instance.remove(); map.current=null; markers.current={}; };
 // Only a different dashboard session may replace the map instance.
 },[mapSession]);
 useLayoutEffect(() => {
  const instance = map.current;
  if (!instance) return;
  const observations = L.layerGroup().addTo(instance);
  const dismiss=()=>dismissInfo(instance,activeInfo);
  const onEscape=(event:KeyboardEvent)=>{if(event.key==='Escape'&&activeInfo.current){event.preventDefault();event.stopPropagation();dismissInfo(instance,activeInfo,true);}};
  const host=instance.getContainer();
  instance.on('click',dismiss);host.addEventListener('keydown',onEscape,true);
  const reference = document.createElement('span'); reference.textContent=synchronized?'Acmac · designated sync shore':'Acmac · area reference';
  L.circleMarker(synchronized?[8.275292,124.262623]:[8.274,124.2649],{radius:5,color:'#526c7e',fillColor:'#fff',fillOpacity:1,weight:2}).bindTooltip(reference,{permanent:true,direction:'right',className:'shore-label'}).addTo(observations);
  for (const device of devices) {
   const position = getLastPosition(readings,device.id,now);
   if (!position) continue;
   const icon = document.createElement('span'); icon.className='boat-pin'; icon.textContent=synchronized?device.id:device.name.replace('Vessel ','');
   const marker = L.marker([position.latitude!,position.longitude!],{
    icon:L.divIcon({className:'vessel-marker',html:icon,iconSize:[44,44],iconAnchor:[22,22]}), alt:`${device.name} last reported ${synchronized?'GPS fix':'sample GPS position'}`, keyboard:true,
   }).addTo(observations);
   marker.getElement()?.setAttribute('aria-label',`Select ${device.name} ${synchronized?'last timed GPS fix':'sample position'}`);
   marker.getElement()?.setAttribute('aria-haspopup','dialog');
   marker.getElement()?.setAttribute('aria-expanded','false');
   marker.on('click',()=>{
    onSelect(device.id);
    dismiss();
    if(studyPhase!=='ready'||(synchronized&&!study))return;
    // Hosted GPS rows and weather rows are intentionally separate projections.
    const weather=synchronized?study?.latestWeather[device.id]:getLatestReading(readings,device.id,now);
    const sos=popupEmergency(device.id,now,synchronized?(study?.records??[]):undefined,sosEvents);
    const {card,close}=popupContent(device,position,weather,sos,synchronized,()=>dismissInfo(instance,activeInfo,true));
    card.style.width=Math.min(246,Math.max(120,Math.floor((instance.getSize().x-40)/2)))+'px';
    const layer=L.tooltip({direction:'auto',offset:[14,0],permanent:true,interactive:true,opacity:1,className:'vessel-info-popup'}).setLatLng(marker.getLatLng()).setContent(card).addTo(instance);
    activeInfo.current={id:device.id,layer,marker};
    const element=layer.getElement();element?.setAttribute('role','dialog');element?.setAttribute('aria-label',device.name+' information');
    marker.getElement()?.setAttribute('aria-expanded','true');
    close.focus({preventScroll:true});
   }); markers.current[device.id]=marker;
  }
  for (const event of sosEvents) {
   const label = document.createElement('span'); label.textContent=`Sample SOS · ${event.deviceId} · event location, not current position`;
   L.circleMarker([event.latitude,event.longitude],{radius:18,color:'#d07153',fillColor:'#f3ad89',fillOpacity:0.15,weight:2,dashArray:'4 4'}).bindTooltip(label).addTo(observations);
  }
  return () => { dismiss(); instance.off('click',dismiss);host.removeEventListener('keydown',onEscape,true);if(instance.hasLayer(observations))instance.removeLayer(observations);markers.current={}; };
 },[devices,readings,sosEvents,now,onSelect,synchronized,mapSession,study,studyPhase]);
 useLayoutEffect(() => {
  Object.entries(markers.current).forEach(([id,marker]) => marker.getElement()?.classList.toggle('is-selected',id===selectedId));
  if(activeInfo.current&&activeInfo.current.id!==selectedId)dismissInfo(map.current,activeInfo);
  // Selection changes highlight only. Explicit map reset remains available.
 },[selectedId,devices,readings,sosEvents,now,onSelect,synchronized,mapSession,study,studyPhase]);
 // The basemap is optional; do not rebuild the marker map when the toggle changes.
 // Change tiles independently of observation and selection updates.
 useEffect(() => {
  setTileError(false);
  const instance = map.current;
  if (!instance || !showTiles) return;
  const layer = L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
   maxZoom:18, attribution:'&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener noreferrer">OpenStreetMap contributors</a>',
  });
  const onError = () => setTileError(true);
  layer.on('tileerror',onError);
  layer.addTo(instance);
  return () => { layer.off('tileerror',onError); if (instance.hasLayer(layer)) instance.removeLayer(layer); };
 },[showTiles,mapSession]);
 const selected = getLastPosition(readings,selectedId,now);
 return <section className="panel vessel-map-panel" id="vessel-map">
  <div className="panel-heading"><div><h2><Navigation size={16}/> Vessel locations <span className="live-label">{synchronized?'LAST TIMED FIX':'SAMPLE POSITIONS'}</span></h2><p>Iligan Bay · Barangay Acmac, Iligan City</p></div><button className="map-reset" onClick={() => map.current?.setView([8.283,124.245],13)} aria-label="Reset map to Acmac area"><LocateFixed size={16}/></button></div>

  <div className="map-container" ref={container} role="region" aria-label={synchronized?'Last timed synchronized vessel GPS fixes':'Interactive map of fictional sample vessel positions near Acmac'}/>
  {showTiles&&tileError&&<div className="map-error" role="status">Some basemap tiles could not load. Vessel markers and available coordinates remain visible.</div>}
  <div className="map-location"><MapPin size={15}/><div><strong>{devices.find(d => d.id===selectedId)?.name??'Vessel positions'} · {studyPhase==='loading'?'Loading':studyPhase==='error'?'Unavailable':selected?'last reported GPS fix':'GPS unavailable'}</strong><span>{studyPhase==='loading'?'Refreshing synchronized vessel positions…':studyPhase==='error'?'Unable to load vessel positions.':studyPhase==='empty'?'No synchronized vessel positions available.':selected?`${selected.latitude!.toFixed(5)}° N, ${selected.longitude!.toFixed(5)}° E · ${ageLabel(selected.timestamp,now)}`:synchronized?'No unambiguous timed valid fix. No marker is shown for this vessel.':'No valid coordinates received. No marker is shown for this vessel.'}</span></div></div>
  <div className="map-caption"><span><i className="legend-vessel"/> {synchronized?'Last timed vessel fix':'Sample vessel'} {!synchronized&&<><i className="legend-sos"/> Sample SOS event location</>}</span><small>{synchronized?(showTiles?'External basemap enabled':mode==='hosted'?'External basemap off · About':'Local review: external basemap disabled'):'Illustrative positions only'}</small></div>
 </section>;
}
