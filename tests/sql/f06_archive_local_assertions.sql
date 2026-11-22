-- SQL integration assertions for disposable local Postgres 17.
-- Synthetic digest is only a database authorization fixture.
-- Edge Function SHA256 byte verification is NOT tested here.
CREATE FUNCTION public.f06_local_payload() RETURNS jsonb
LANGUAGE sql IMMUTABLE AS $fixture$
  SELECT jsonb_build_array(jsonb_build_object(
    'ordinal', 1,
    'raw_line', '{"kind":"sos_rx","vessel_id":2,"boot_token":12,"event_index":4,"uptime_ms":123}',
    'body', jsonb_build_object('kind','sos_rx','vessel_id',2,
             'boot_token',12,'event_index',4,'uptime_ms',123),
    'boot_token',12,'event_index',4
  ));
$fixture$;
GRANT EXECUTE ON FUNCTION public.f06_local_payload() TO service_role;

-- Gate 1: all three new tables require RLS, no public data grants.
DO $check$
DECLARE name text;
BEGIN
  FOREACH name IN ARRAY ARRAY[
    'dtn_f06_approved_exports','dtn_f06_history_files','dtn_f06_history_records'
  ] LOOP
    IF NOT EXISTS (
      SELECT 1 FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace
      WHERE c.relname=name AND n.nspname='public' AND c.relrowsecurity
    ) THEN
      RAISE EXCEPTION 'RLS disabled or table missing: %',name;
    END IF;
    IF has_table_privilege('anon',format('public.%I',name),'SELECT')
      OR has_table_privilege('anon',format('public.%I',name),'INSERT')
      OR has_table_privilege('authenticated',format('public.%I',name),'SELECT')
      OR has_table_privilege('authenticated',format('public.%I',name),'INSERT')
    THEN
      RAISE EXCEPTION 'unapproved public privilege: %',name;
    END IF;
  END LOOP;
  IF has_table_privilege('service_role','public.dtn_f06_approved_exports','INSERT')
    OR has_table_privilege('service_role','public.dtn_f06_approved_exports','UPDATE')
    OR has_table_privilege('service_role','public.dtn_f06_approved_exports','DELETE')
  THEN
    RAISE EXCEPTION 'service role can self-approve a manifest';
  END IF;
  -- Historical archived rows are append-only for the importer.
  -- An identical retry must not require widening service_role privileges.
  IF has_table_privilege('service_role','public.dtn_f06_history_files','UPDATE')
    OR has_table_privilege('service_role','public.dtn_f06_history_files','DELETE')
    OR has_table_privilege('service_role','public.dtn_f06_history_records','UPDATE')
    OR has_table_privilege('service_role','public.dtn_f06_history_records','DELETE')
  THEN
    RAISE EXCEPTION 'historical archives must stay immutable to importer';
  END IF;
  IF has_function_privilege('anon',
    'public.dtn_archive_f06_sos_file(uuid,integer,text,text,integer,jsonb)','EXECUTE')
     OR has_function_privilege('authenticated',
    'public.dtn_archive_f06_sos_file(uuid,integer,text,text,integer,jsonb)','EXECUTE')
  THEN
    RAISE EXCEPTION 'unapproved public RPC execution';
  END IF;
END;
$check$;
\echo PASS 1/7: RLS, table grants, approval separation and RPC grants

-- Gate 2: self-declared SHA256 without an independent manifest is denied.
SET ROLE service_role;
DO $check$
DECLARE rejected boolean := false; message text;
BEGIN
  BEGIN
    PERFORM public.dtn_archive_f06_sos_file(
      '00000000-0000-4000-8000-000000000001'::uuid,
      2,'S58BAFE2',repeat('A',64),100,public.f06_local_payload());
  EXCEPTION WHEN OTHERS THEN
    GET STACKED DIAGNOSTICS message = MESSAGE_TEXT;
    IF message LIKE '%closed-export manifest not independently approved%' THEN
      rejected := true;
    ELSE
      RAISE EXCEPTION 'unexpected error: %',message;
    END IF;
  END;
  IF NOT rejected THEN RAISE EXCEPTION 'unapproved file accepted'; END IF;
END;
$check$;
RESET ROLE;
\echo PASS 2/7: unapproved file rejected

-- Approved digest is created ONLY by the database administrator fixture.
INSERT INTO public.dtn_f06_approved_exports
  (vessel_number,session_name,file_sha256,original_bytes,record_count,approved_by)
VALUES (2,'S58BAFE2',repeat('A',64),100,1,
  '00000000-0000-4000-8000-000000000001'::uuid);

-- Gate 3: first import commits the file and the corresponding event.
SET ROLE service_role;
DO $check$
DECLARE state text;
BEGIN
  SELECT public.dtn_archive_f06_sos_file(
    '00000000-0000-4000-8000-000000000001'::uuid,
    2,'S58BAFE2',repeat('A',64),100,public.f06_local_payload())->>'status'
    INTO state;
  IF state IS DISTINCT FROM 'accepted' THEN
    RAISE EXCEPTION 'first import was not accepted: %',state;
  END IF;
END;
$check$;
RESET ROLE;
DO $check$
BEGIN
  IF (SELECT count(*) FROM public.dtn_f06_history_files) <> 1
    OR (SELECT count(*) FROM public.dtn_f06_history_records) <> 1
  THEN RAISE EXCEPTION 'first import counts not exactly 1/1'; END IF;
END;
$check$;
\echo PASS 3/7: first approved archive accepted, exactly one row

-- Gate 4: identical file replay is acknowledged without a new row.
SET ROLE service_role;
DO $check$
DECLARE state text;
BEGIN
  SELECT public.dtn_archive_f06_sos_file(
    '00000000-0000-4000-8000-000000000001'::uuid,
    2,'S58BAFE2',repeat('A',64),100,public.f06_local_payload())->>'status'
    INTO state;
  IF state IS DISTINCT FROM 'duplicate' THEN
    RAISE EXCEPTION 'repeat import was not duplicate: %',state;
  END IF;
END;
$check$;
RESET ROLE;
DO $check$
BEGIN
  IF (SELECT count(*) FROM public.dtn_f06_history_files) <> 1
    OR (SELECT count(*) FROM public.dtn_f06_history_records) <> 1
  THEN RAISE EXCEPTION 'duplicate retry created extra data'; END IF;
END;
$check$;
\echo PASS 4/7: idempotent identical replay

-- Gate 5: an altered file hash under the existing session cannot pass.
SET ROLE service_role;
DO $check$
DECLARE rejected boolean := false; message text;
BEGIN
  BEGIN
    PERFORM public.dtn_archive_f06_sos_file(
      '00000000-0000-4000-8000-000000000001'::uuid,
      2,'S58BAFE2',repeat('B',64),100,public.f06_local_payload());
  EXCEPTION WHEN OTHERS THEN
    GET STACKED DIAGNOSTICS message = MESSAGE_TEXT;
    IF message LIKE '%closed-export manifest not independently approved%' THEN
      rejected := true;
    ELSE RAISE EXCEPTION 'unexpected error: %',message;
    END IF;
  END;
  IF NOT rejected THEN RAISE EXCEPTION 'changed source accepted'; END IF;
END;
$check$;
RESET ROLE;
\echo PASS 5/7: unauthorized altered-session file rejected

-- Gate 6: same (vessel,boot,event) in another approved closed session
-- must rollback all attempted rows, not leave a partial import.
INSERT INTO public.dtn_f06_approved_exports
  (vessel_number,session_name,file_sha256,original_bytes,record_count,approved_by)
VALUES (2,'SB1F7223',repeat('B',64),100,1,
  '00000000-0000-4000-8000-000000000001'::uuid);
SET ROLE service_role;
DO $check$
DECLARE rejected boolean := false; code text;
BEGIN
  BEGIN
    PERFORM public.dtn_archive_f06_sos_file(
      '00000000-0000-4000-8000-000000000001'::uuid,
      2,'SB1F7223',repeat('B',64),100,public.f06_local_payload());
  EXCEPTION WHEN unique_violation THEN
    GET STACKED DIAGNOSTICS code = RETURNED_SQLSTATE;
    rejected := code = '23505';
  END;
  IF NOT rejected THEN RAISE EXCEPTION 'cross-session event collision not blocked'; END IF;
END;
$check$;
RESET ROLE;
DO $check$
BEGIN
  IF (SELECT count(*) FROM public.dtn_f06_history_files) <> 1
    OR (SELECT count(*) FROM public.dtn_f06_history_records) <> 1
  THEN RAISE EXCEPTION 'failed collision left partial archive data'; END IF;
END;
$check$;
\echo PASS 6/7: cross-session collision rolls back atomically

-- Gate 7: this archive cannot activate or resolve an operational SOS.
DO $check$
BEGIN
  IF (SELECT count(*) FROM public.emergency_alerts) <> 0 THEN
    RAISE EXCEPTION 'historical import changed emergency_alerts';
  END IF;
  IF (SELECT count(*) FROM public.telemetry) <> 0 THEN
    RAISE EXCEPTION 'historical import changed telemetry';
  END IF;
END;
$check$;
\echo PASS 7/7: active SOS and operational telemetry unchanged
\echo === LOCAL F06 DATABASE TESTS PASS 7/7 ===
