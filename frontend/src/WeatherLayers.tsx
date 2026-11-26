import {CloudSun,Activity} from 'lucide-react';
import type {Row} from './studySf';
import type {SnapshotPhase} from './refreshState';
import {recordedWeather,microStateLabel,microPatternLabel} from './weatherPresentation';
type Props={rows?:Row[];nodeId:string;now:number;phase?:SnapshotPhase;compact?:boolean};
const stamp=(t:number|null)=>t===null?'Unavailable':new Date(t).toLocaleString('en-PH',{timeZone:'Asia/Manila',year:'numeric',month:'short',day:'numeric',hour:'2-digit',minute:'2-digit',second:'2-digit'})+' PHT';
export default function WeatherLayers({rows,nodeId,now,phase='ready',compact=false}:Props){
 const pending=phase==='loading',failed=phase==='error';
 const recorded=recordedWeather(phase==='ready'?rows:undefined,nodeId,now);
 const micro=recorded.micro;
 const state=pending?'Loading…':failed?'Unavailable':microStateLabel(micro);
 const time=pending?'Loading…':stamp(recorded.captureAt);
 const receipt=pending?'Loading…':stamp(recorded.receiptAt);
 const unavailable=pending?'Loading…':'Unavailable';
 return <section className={'weather-layers'+(compact?' weather-layers-compact':'')} aria-label="Recorded dual-layer weather outputs" aria-busy={pending}>
  <article className="weather-layer panel">
   <header><CloudSun size={17}/><h3>Macro outlook</h3><span>Zambretti</span></header>
   <strong className="weather-layer-result">{unavailable}</strong>
   <p>{rows===undefined?'Independent macro output is not included in the fictional samples.':'Independent macro output is not included in the accepted DTN weather record.'}</p>
   {!compact&&<dl><div><dt>Zambretti index / forecast</dt><dd>{unavailable}</dd></div><div><dt>Computed at</dt><dd>{unavailable}</dd></div><div><dt>Forecast horizon / elapsed time</dt><dd>{unavailable}</dd></div></dl>}
  </article>
  <article className="weather-layer panel">
   <header><Activity size={17}/><h3>Micro nowcast</h3><span>Sensor fusion</span></header>
   <strong className="weather-layer-result">{state}</strong>
   <dl><div><dt>Recorded pattern</dt><dd>{pending?'Loading…':microPatternLabel(micro)}</dd></div><div><dt>Weather record capture</dt><dd>{time}</dd></div>{!compact&&<><div><dt>Input validity</dt><dd>{pending?'Loading…':micro?micro.inputValid?'Valid input recorded':'Invalid input recorded':'Unavailable'}</dd></div><div><dt>Backend receipt</dt><dd>{receipt}</dd></div><div><dt>Nowcast computed at</dt><dd>{unavailable}</dd></div></>}</dl>
   <p>{rows===undefined?'No synchronized micro result in demo mode.':'Recorded historical output; no-pattern or valid input does not establish safe conditions.'}</p>
  </article>
 </section>;
}
