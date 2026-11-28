import type {ReactNode} from 'react';
import {AlertCircle,ChevronLeft,ChevronRight,Inbox,LoaderCircle,Search} from 'lucide-react';
import type {SnapshotPhase} from './refreshState';

export const PAGE_SIZE=6;
export function TableState({phase,empty,filtered=false}:{phase:SnapshotPhase;empty:boolean;filtered?:boolean}){
 const loading=phase==='loading',error=phase==='error',nomatch=!empty&&filtered;
 const Icon=loading?LoaderCircle:error?AlertCircle:nomatch?Search:Inbox;
 return <div className={'record-table-state'+(loading?' is-loading':'')} role="status"><Icon size={23}/><strong>{loading?'Loading records…':error?'Unable to load records':nomatch?'No matching records':'No records available'}</strong><span>{loading?'Retrieving synchronized observations.':error?'Use Refresh or Retry to retrieve records again.':nomatch?'Try changing the search or filters.':'No observations are available in this dataset.'}</span></div>;
}
export function TablePagination({page,count,onPage,disabled=false}:{page:number;count:number;onPage:(p:number)=>void;disabled?:boolean}){
 const pages=Math.max(1,Math.ceil(count/PAGE_SIZE)),current=Math.min(page,pages-1);
 return <div className="table-footer"><span aria-live="polite">{disabled?'—':count?`Showing ${current*PAGE_SIZE+1}–${Math.min((current+1)*PAGE_SIZE,count)} of ${count} records`:'0 matching records'}</span><div className="pagination"><button disabled={disabled||current===0} onClick={()=>onPage(current-1)} aria-label="Previous page"><ChevronLeft size={15}/></button><span>Page {current+1} of {pages}</span><button disabled={disabled||current+1>=pages} onClick={()=>onPage(current+1)} aria-label="Next page"><ChevronRight size={15}/></button></div></div>;
}
export function RecordTableBody({children,columns,count,phase,empty,filtered=false}:{children:ReactNode;columns:number;count:number;phase:SnapshotPhase;empty:boolean;filtered?:boolean}){
 return <tbody>{phase==='loading'||phase==='error'||count===0?<tr className="record-state-row"><td colSpan={columns}><TableState phase={phase} empty={empty} filtered={filtered}/></td></tr>:<>{children}{Array.from({length:PAGE_SIZE-count},(_,i)=><tr className="record-spacer-row" key={`spacer-${i}`} aria-hidden="true"><td colSpan={columns}/></tr>)}</>}</tbody>;
}
