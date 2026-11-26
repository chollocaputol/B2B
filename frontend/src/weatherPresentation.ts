import {latestTimed,type Row} from './studySf.ts';

export type MicroObservation={inputValid:boolean;state:string;pattern:string};
export type RecordedWeather={captureAt:number|null;receiptAt:number|null;micro:MicroObservation|null};
const unavailable=():RecordedWeather=>({captureAt:null,receiptAt:null,micro:null});
function microObservation(value:unknown):MicroObservation|null {
 if(!value||typeof value!=='object'||Array.isArray(value))return null;
 const p=value as Record<string,unknown>;
 if(typeof p.input_valid!=='boolean'||typeof p.state!=='string'||typeof p.pattern!=='string'||!p.state.trim()||!p.pattern.trim()||p.state.length>64||p.pattern.length>64)return null;
 return {inputValid:p.input_valid,state:p.state,pattern:p.pattern};
}
// The accepted DTN contract has no independent Zambretti result, computation
// timestamp, forecast horizon or micro computation timestamp. Do not infer them.
export function recordedWeather(rows:Row[]|undefined,nodeId:string,now:number):RecordedWeather {
 if(!rows||!nodeId||!Number.isFinite(now))return unavailable();
 const row=latestTimed(rows.filter(r=>String(r.recorder_node_id)===nodeId&&r.record_json.kind==='weather'&&r.record_json.captured_at!==null&&Date.parse(r.record_json.captured_at)<=now));
 if(!row)return unavailable();
 const receipt=Date.parse(row.synced_at);
 return {captureAt:Date.parse(row.record_json.captured_at!),receiptAt:Number.isFinite(receipt)&&receipt<=now?receipt:null,micro:microObservation(row.record_json.payload.micro)};
}
export function microStateLabel(micro:MicroObservation|null):string {
 if(!micro)return 'Unavailable';
 if(!micro.inputValid)return 'Invalid input';
 if(micro.state==='WARMUP')return 'Warm-up';
 if(micro.state==='INVALID')return 'Invalid state';
 if(micro.state==='NO_PATTERN')return 'No pattern recorded';
 return micro.state;
}
// Only show a pattern as a result when the recorded inputs and state support it.
export function microPatternLabel(micro:MicroObservation|null):string {
 if(!micro||!micro.inputValid||micro.state==='WARMUP'||micro.state==='INVALID')return 'Unavailable';
 return micro.pattern==='NONE'?'None recorded':micro.pattern;
}
