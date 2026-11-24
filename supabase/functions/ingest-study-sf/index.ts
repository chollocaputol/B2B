// LOCAL STUDY CANDIDATE ONLY. Do not deploy. Default denial unless explicitly configured.
import { createClient } from 'npm:@supabase/supabase-js@2.95.0';
import { handleStudySf } from '../_shared/study_sf_http.ts';
Deno.serve(async req=>{
  const url=Deno.env.get('SUPABASE_URL');
  const publicKey=Deno.env.get('SUPABASE_ANON_KEY');
  const adminKey=Deno.env.get('SUPABASE_SERVICE_ROLE_KEY');
  // Explicit lab allowlist, never a hosted URL. Local Docker Kong is the expected host.
  if(Deno.env.get('THESIS_SF_LOCAL_LAB')!=='1'||url!=='http://kong:8000'||!publicKey||!adminKey)
    return Response.json({error:'study_lab_disabled'},{status:503});
  const opts={auth:{autoRefreshToken:false,persistSession:false}};
  return handleStudySf(req,{
    getRecorder: async token=>{
      const {data,error}=await createClient(url,publicKey,opts).auth.getUser(token);
      if(error||!data.user||data.user.app_metadata?.thesis_study_device!==true)return null;
      // Server-managed app metadata, never user_metadata or a request claim.
      const node=data.user.app_metadata?.thesis_node_id;
      return Number.isInteger(node)&&node>=1&&node<=9999?node:null;
    },
    store: async (recorder,records)=>{
      const {data,error}=await createClient(url,adminKey,opts).rpc('study_sf_ingest',{p_recorder:recorder,p_records:records});
      return {data,error};
    },
  });
});
