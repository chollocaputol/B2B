import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import StudyAccess from './StudyAccess';
import {entryRoute,operatorDestination} from './routes';
import './styles.css';

function Landing({missing=false}:{missing?:boolean}) {
 return <main style={{maxWidth:800,margin:'auto',padding:24}}>
  <div className="page-heading"><div><h1>{missing?'Page not found':'Acmac vessel & weather monitoring'}</h1><p>Barangay Acmac · Iligan City</p></div></div>
  <section className="panel" style={{padding:24}}>
   <p>{missing?'Use the operator entry to continue.':'LoRa-based geolocation and weather assistance for small-scale fishermen. Authorized personnel can review synchronized vessel observations and recorded emergency reports.'}</p>
   <p>Not a live tracking or rescue dispatch service.</p>
   <a className="button primary-button" href="/login">Operator sign-in</a>
  </section>
 </main>;
}
const entry=entryRoute(location.pathname,location.search,location.hostname,import.meta.env.DEV);
if(entry==='legacy') history.replaceState(null,'',operatorDestination('/dashboard',location.hash));
// Revalidate a restored document instead of reusing a cached protected screen.
window.addEventListener('pageshow',e=>{if(e.persisted && (entry==='operator'||entry==='legacy')) location.reload();});
ReactDOM.createRoot(document.getElementById('root')!).render(<React.StrictMode>{entry==='operator'||entry==='legacy'?<StudyAccess/>:entry==='demo'?<App/>:<Landing missing={entry==='not-found'}/>}</React.StrictMode>);
