// Approved hosted thesis project or explicitly enabled isolated local lab.
// Use the built-in HTTP client so the offline worker needs no external npm modules.
import { handleStudySf } from '../_shared/study_sf_http.ts';
const object = (x: unknown): x is Record<string, unknown> => x !== null && typeof x === 'object' && !Array.isArray(x);
Deno.serve(async req => {
  const url = Deno.env.get('SUPABASE_URL');
  const publicKey = Deno.env.get('SUPABASE_ANON_KEY');
  const adminKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY');
  if (!url || !(url === 'https://twqumqiplqqfhixjvxeo.supabase.co' ||
      (url === 'http://kong:8000' && Deno.env.get('THESIS_SF_LOCAL_LAB') === '1')) ||
      !publicKey || !adminKey || publicKey === adminKey)
    return Response.json({error:'study_lab_disabled'},{status:503});
  return handleStudySf(req, {
    getRecorder: async token => {
      const res = await fetch(url + '/auth/v1/user', {
        method:'GET', headers:{apikey:publicKey,authorization:`Bearer ${token}`},
        redirect:'error', signal:AbortSignal.timeout(10000),
      });
      if (res.status === 401 || res.status === 403) return null;
      if (!res.ok) throw new Error('Auth unavailable');
      const user: unknown = await res.json();
      if (!object(user) || typeof user.id !== 'string' || !user.id) throw new Error('Invalid Auth response');
      // Only server-managed metadata authorizes a device. Never trust user_metadata.
      if (!object(user.app_metadata) || user.app_metadata.thesis_study_device !== true || user.app_metadata.thesis_study_reviewer === true) return null;
      const node = user.app_metadata.thesis_node_id;
      return typeof node === 'number' && Number.isInteger(node) && node >= 1 && node <= 9999 ? node : null;
    },
    store: async (recorder, records) => {
      const res = await fetch(url + '/rest/v1/rpc/study_sf_ingest', {
        method:'POST', headers:{apikey:adminKey,authorization:`Bearer ${adminKey}`,'content-type':'application/json',accept:'application/json'},
        body:JSON.stringify({p_recorder:recorder,p_records:records}),
        redirect:'error', signal:AbortSignal.timeout(10000),
      });
      const data: unknown = await res.json();
      if (!res.ok) return {data:null,error:{code:res.status === 409 && object(data) && data.code === '23505' ? '23505' : 'unavailable'}};
      return {data,error:null};
    },
  });
});
