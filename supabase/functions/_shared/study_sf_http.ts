// Study-only Store-and-Forward HTTP boundary. No physical or cloud authorization.
// One persisted record identity, bounded atomic batch, exact per-record confirmation.
export type RecordJson = Record<string, unknown>;
export type Receipt = { record_id: string; status: 'accepted' | 'duplicate' };
export type Dependencies = {
  getRecorder: (token: string) => Promise<number | null>;
  store: (recorder: number, records: RecordJson[]) => Promise<{data: unknown; error?: {code?: string} | null}>;
};
const MAX_BODY = 16384;
const MAX_BATCH = 8;
const uuid = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/;
const id = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}:[1-9][0-9]{0,9}$/;
const obj = (x: unknown): x is RecordJson => x !== null && typeof x === 'object' && !Array.isArray(x);
const int = (x: unknown, min: number, max: number): x is number => typeof x === 'number' && Number.isInteger(x) && x >= min && x <= max;
const nullable = (x: unknown, min: number, max: number) => x === null || (typeof x === 'number' && Number.isFinite(x) && x >= min && x <= max);
const keys = (x: RecordJson, list: string[]) => Object.keys(x).length === list.length && list.every(k => Object.hasOwn(x,k));
const response = (status: number, data: unknown) => new Response(JSON.stringify(data), {status, headers: {'content-type':'application/json','cache-control':'no-store'}});
function validUtc(x: unknown): boolean {
  if (x === null) return true;
  if (typeof x !== 'string' || !/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$/.test(x)) return false;
  const t = Date.parse(x); return Number.isFinite(t) && new Date(t).toISOString() === x.replace('Z','.000Z');
}
const common = ['record_id','kind','recorder_node_id','captured_at','uptime_ms','payload'];
export function validRecord(x: unknown, recorder: number): x is RecordJson {
  if (!obj(x) || !keys(x,common) || typeof x.record_id !== 'string' || !id.test(x.record_id) ||
      !int(Number(x.record_id.split(':')[1]),1,4294967295) || x.recorder_node_id !== recorder ||
      !validUtc(x.captured_at) || !int(x.uptime_ms,0,4294967295) || !obj(x.payload)) return false;
  const p=x.payload;
  if (x.kind==='weather') {
    const legacy=['temperature_c','humidity_percent','pressure_hpa','pressure_trend_hpa','weather_state','micro'];
    const macro=p.macro;
    const extended=keys(p,[...legacy,'macro']) && (
      (macro===null && obj(p.micro)) || (p.micro===null && obj(macro) &&
        keys(macro,['input_valid','history_count','state','zambretti_index']) &&
        typeof macro.input_valid==='boolean' && int(macro.history_count,0,10) &&
        int(macro.state,0,3) && macro.state===p.weather_state &&
        (macro.zambretti_index===null ? (!macro.input_valid || macro.history_count!==10 || macro.state===0 || p.pressure_trend_hpa===null) :
          (macro.input_valid===true && macro.history_count===10 && macro.state!==0 && typeof p.pressure_trend_hpa==='number' && int(macro.zambretti_index,0,255)))));
    return (keys(p,legacy) || extended) &&
      nullable(p.temperature_c,-100,100) && nullable(p.humidity_percent,0,100) && nullable(p.pressure_hpa,100,1200) &&
      nullable(p.pressure_trend_hpa,-200,200) && int(p.weather_state,0,3) &&
      (p.micro===null || (obj(p.micro) && keys(p.micro,['input_valid','state','pattern']) && typeof p.micro.input_valid==='boolean' &&
        typeof p.micro.state==='string' && p.micro.state.length<=64 && typeof p.micro.pattern==='string' && p.micro.pattern.length<=64));
  }
  if (x.kind==='gps') return (keys(p,['gps_fix','latitude','longitude']) || (keys(p,['gps_fix','latitude','longitude','battery_percent']) && (p.battery_percent===null || int(p.battery_percent,0,100)))) && typeof p.gps_fix==='boolean' &&
    (p.gps_fix ? p.latitude!==null && p.longitude!==null && nullable(p.latitude,-90,90) && nullable(p.longitude,-180,180) : p.latitude===null && p.longitude===null);
  if (x.kind!=='sos' && x.kind!=='cancel') return false;
  if (!keys(p,['origin_node_id','event_id','source','packet_hex']) || !int(p.origin_node_id,1,9999) ||
    !int(p.event_id,1,4294967295) || !['local','received'].includes(p.source as string) ||
    (p.source==='local' && p.origin_node_id!==recorder) || typeof p.packet_hex!=='string' || !/^[0-9a-f]{124}$/.test(p.packet_hex)) return false;
  // Preserve the accepted 62-byte VesselPacket ABI. No reconstruction from F06 metadata.
  const b = Uint8Array.from(p.packet_hex.match(/../g)!,s=>parseInt(s,16));
  const v = new DataView(b.buffer);
  return v.getUint16(0,true)===0x4d45 && b[2]===1 && b[3]===(x.kind==='sos'?2:4) &&
    v.getUint32(4,true)===p.origin_node_id && v.getUint32(8,true)===p.event_id &&
    (x.kind==='sos' || (v.getUint32(54,true)===p.event_id && v.getUint32(58,true)===p.origin_node_id));
}
async function body(req: Request): Promise<unknown> {
  const n=req.headers.get('content-length');
  if (n!==null && (!/^\d+$/.test(n) || Number(n)>MAX_BODY)) throw new Error('size');
  if (!req.body) throw new Error('body');
  const reader=req.body.getReader(); let length=0; const chunks:Uint8Array[]=[];
  while (true) { const x=await reader.read(); if(x.done) break; length+=x.value.length;
    if(length>MAX_BODY){await reader.cancel();throw new Error('size');} chunks.push(x.value); }
  const bytes=new Uint8Array(length);let offset=0;for(const c of chunks){bytes.set(c,offset);offset+=c.length;}
  return JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes));
}
export async function handleStudySf(req: Request,deps: Dependencies): Promise<Response> {
  if(req.method!=='POST') return response(405,{error:'method_not_allowed'});
  const token=/^Bearer ([A-Za-z0-9._-]+)$/.exec(req.headers.get('authorization')??'');
  if(!token) return response(401,{error:'device_auth_required'});
  if(!/^application\/json(?:\s*;|$)/i.test(req.headers.get('content-type')??'')) return response(415,{error:'json_required'});
  let recorder:number|null;try{recorder=await deps.getRecorder(token[1]);}catch{return response(503,{error:'auth_unavailable'});}
  if(!int(recorder,1,9999))return response(403,{error:'device_not_authorized'});
  let x:unknown;try{x=await body(req);}catch{return response(400,{error:'invalid_or_oversize_body'});}
  if(!obj(x)||!keys(x,['schema_version','batch_id','records'])||x.schema_version!==1||typeof x.batch_id!=='string'||!uuid.test(x.batch_id)||
    !Array.isArray(x.records)||x.records.length<1||x.records.length>MAX_BATCH||!x.records.every(r=>validRecord(r,recorder))||
    new Set(x.records.map(r=>r.record_id)).size!==x.records.length) return response(422,{error:'invalid_batch'});
  let saved:Awaited<ReturnType<Dependencies['store']>>;try{saved=await deps.store(recorder,x.records);}catch{return response(503,{error:'store_unavailable'});}
  if(saved.error)return response(saved.error.code==='23505'?409:503,{error:saved.error.code==='23505'?'record_identity_conflict':'store_unavailable'});
  if(!Array.isArray(saved.data)||saved.data.length!==x.records.length||!saved.data.every((r,i)=>obj(r)&&keys(r,['record_id','status'])&&r.record_id===(x.records as RecordJson[])[i].record_id&&(r.status==='accepted'||r.status==='duplicate')))
    return response(503,{error:'invalid_storage_confirmation'});
  return response(200,{schema_version:1,batch_id:x.batch_id,recorder_node_id:recorder,records:saved.data});
}
