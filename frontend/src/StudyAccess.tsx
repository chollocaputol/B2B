import {useState} from 'react';
import App from './App';
import {getSupabaseClient} from './lib/supabase';
import type {Row} from './studySf';
import {excludeKnownFixtures} from './studyDashboardData';
export default function StudyAccess(){
 const [logoutFailed,setLogoutFailed]=useState(false);
 const [excluded,setExcluded]=useState(0);
 const [email,setEmail]=useState(''),[password,setPassword]=useState(''),[rows,setRows]=useState<Row[]|null>(null),[signed,setSigned]=useState(false),[busy,setBusy]=useState(false),[status,setStatus]=useState('Sign in as the local study reviewer.');
 const local=location.hostname==='127.0.0.1'&&import.meta.env.VITE_SUPABASE_URL==='http://127.0.0.1:54321';
 async function load(login:boolean){if(!local||busy)return;setBusy(true);setRows(null);setLogoutFailed(false);try{const c=getSupabaseClient();if(login){const {error}=await c.auth.signInWithPassword({email,password});setPassword('');if(error)throw Error('Sign-in failed');}
 const {data:user,error:authError}=await c.auth.getUser();if(authError||user.user?.app_metadata.thesis_study_reviewer!==true||user.user.app_metadata.thesis_study_device===true)throw Error('Reviewer authorization required');
 const out:Row[]=[];for(let start=0;start<=10000;start+=500){const {data,error}=await c.from('study_sf_records').select('recorder_node_id,record_id,record_json,synced_at').order('recorder_node_id').order('record_id').range(start,start+499);if(error||!data)throw Error('Study read failed');out.push(...(data as Row[]));if(out.length>10000)throw Error('Study limit exceeded; no partial snapshot shown');if(data.length<500){const view=excludeKnownFixtures(out);setRows(view.records);setExcluded(view.excluded);setSigned(true);setStatus(`${view.records.length} synchronized non-fixture records; ${view.excluded} known test records excluded.`);return;}}
 throw Error('Study limit exceeded; no partial snapshot shown');
 }catch(e){setRows(null);setSigned(false);setPassword('');setStatus(e instanceof Error?e.message:'Study read failed');try{await getSupabaseClient().auth.signOut();}catch{/* data already cleared */}}finally{setBusy(false);}}
 async function logout(){if(busy)return;setRows(null);setSigned(false);setPassword('');setBusy(true);try{const {error}=await getSupabaseClient().auth.signOut();setLogoutFailed(!!error);setStatus(error?'Records cleared; session sign-out failed. Retry sign-out.':'Signed out.');}catch{setLogoutFailed(true);setStatus('Records cleared; session sign-out failed.');}finally{setBusy(false);}}
 const controls=<><button className="button" disabled={busy} onClick={()=>void load(false)}>Refresh</button> <button className="button" disabled={busy} onClick={()=>void logout()}>Sign out</button></>;
 if(local&&signed&&rows?.length)return <App studyRows={rows} studyControls={controls} excludedFixtures={excluded}/>;
 return <main style={{maxWidth:800,margin:'auto',padding:24}}><div className="page-heading"><div><h1>Vessels &amp; weather</h1><p>Local authenticated study access · synchronized history only.</p></div><a href="/">Demo dashboard</a></div><section className="panel" style={{padding:24}}>{!local?<p>Study access is disabled outside the isolated local laboratory.</p>:logoutFailed?<button className="button" disabled={busy} onClick={()=>void logout()}>Retry sign-out</button>:signed?controls:<form onSubmit={e=>{e.preventDefault();void load(true)}}><label>Email <input type="email" value={email} onChange={e=>setEmail(e.target.value)} required/></label> <label>Password <input type="password" value={password} onChange={e=>setPassword(e.target.value)} required/></label> <button className="button" disabled={busy}>Sign in</button></form>}<p role="status">{busy?'Working…':status}</p></section></main>;
}
