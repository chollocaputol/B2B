import {CloudSun,Activity} from 'lucide-react';
import type {Row} from './studySf';
import type {SnapshotPhase} from './refreshState';
import {recordedWeather,microStateLabel,microPatternLabel} from './weatherPresentation';
import {recordedMacro,macroResultLabel,macroIndexLabel,macroRecordedState} from './wxbatPresentation';
type Props={rows?:Row[];nodeId:string;now:number;phase?:SnapshotPhase;compact?:boolean};
const stamp=(t:number|null)=>t===null?'Unavailable':new Date(t).toLocaleString('en-PH',{timeZone:'Asia/Manila',year:'numeric',month:'short',day:'numeric',hour:'2-digit',minute:'2-digit',second:'2-digit'})+' PHT';
export default function WeatherLayers({rows,nodeId,now,phase='ready',compact=false}:Props){
 const pending=phase==='loading';
 const eligible=phase==='ready'?rows:undefined;
 const recorded=recordedWeather(eligible,nodeId,now),macroRecord=recordedMacro(eligible,nodeId,now);
 const micro=recorded.micro,macro=macroRecord.macro;
 const display=(value:string)=>pending?'Loading…':value;
 const validity=(m:{inputValid:boolean}|null)=>m?m.inputValid?'Valid input recorded':'Invalid input recorded':'Unavailable';
 return <section className={'weather-layers'+(compact?' weather-layers-compact':'')} aria-label="Recorded dual-layer weather outputs" aria-busy={pending}>
  <article className="weather-layer panel">
   <header><CloudSun size={17}/><h3>Macro outlook</h3><span>Zambretti</span></header>
   <strong className="weather-layer-result">{display(macroResultLabel(macro))}</strong>
   <dl><div><dt>Zambretti index</dt><dd>{display(macroIndexLabel(macro))}</dd></div><div><dt>Macro capture</dt><dd>{display(stamp(macroRecord.captureAt))}</dd></div>{!compact&&<><div><dt>Input validity</dt><dd>{display(validity(macro))}</dd></div><div><dt>Pressure history</dt><dd>{display(macro?`${macro.historyCount}/10`:'Unavailable')}</dd></div><div><dt>Recorded macro state</dt><dd>{display(macroRecordedState(macro))}</dd></div><div><dt>Backend receipt</dt><dd>{display(stamp(macroRecord.receiptAt))}</dd></div><div><dt>Computed at / forecast horizon</dt><dd>{display('Unavailable')}</dd></div></>}</dl>
   <p>{rows===undefined?'No independent macro output in demo mode.':'Recorded historical output; raw index is not an official forecast or safety assessment.'}</p>
  </article>
  <article className="weather-layer panel">
   <header><Activity size={17}/><h3>Micro nowcast</h3><span>Sensor fusion</span></header>
   <strong className="weather-layer-result">{display(microStateLabel(micro))}</strong>
   <dl><div><dt>Recorded pattern</dt><dd>{display(microPatternLabel(micro))}</dd></div><div><dt>Micro capture</dt><dd>{display(stamp(recorded.captureAt))}</dd></div>{!compact&&<><div><dt>Input validity</dt><dd>{display(validity(micro))}</dd></div><div><dt>Backend receipt</dt><dd>{display(stamp(recorded.receiptAt))}</dd></div><div><dt>Nowcast computed at</dt><dd>{display('Unavailable')}</dd></div></>}</dl>
   <p>{rows===undefined?'No synchronized micro result in demo mode.':'Experimental historical output; no-pattern or valid input does not establish safe conditions.'}</p>
  </article>
 </section>;
}
