-- Synthetic local database only. Run after existing initial migration and study_sf_ingest.sql.
-- The fixture is rolled back; no actual vessel data is used.
BEGIN;
INSERT INTO public.vessels(node_id,registration_serial_no,vessel_name) VALUES (9998,'SF-SYNTH-9998','Synthetic SF node');
DO $$ BEGIN
  IF NOT (SELECT relrowsecurity FROM pg_class WHERE oid='public.study_sf_records'::regclass)
     OR has_table_privilege('anon','public.study_sf_records','SELECT')
     OR has_table_privilege('authenticated','public.study_sf_records','INSERT')
     OR has_function_privilege('anon','public.study_sf_ingest(bigint,jsonb)','EXECUTE')
     OR has_function_privilege('authenticated','public.study_sf_ingest(bigint,jsonb)','EXECUTE')
     OR has_table_privilege('service_role','public.study_sf_records','UPDATE') THEN RAISE EXCEPTION 'permission assertions failed'; END IF;
END $$;
SET LOCAL ROLE service_role;
DO $$ DECLARE
  a jsonb:='{"record_id":"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:1","kind":"weather","recorder_node_id":9998,"captured_at":null,"uptime_ms":10,"payload":{"temperature_c":29,"humidity_percent":78,"pressure_hpa":1009,"pressure_trend_hpa":null,"weather_state":1,"micro":null}}';
  b jsonb:='{"record_id":"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:2","kind":"gps","recorder_node_id":9998,"captured_at":null,"uptime_ms":20,"payload":{"gps_fix":false,"latitude":null,"longitude":null}}';
  c jsonb:='{"record_id":"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:3","kind":"sos","recorder_node_id":9998,"captured_at":null,"uptime_ms":30,"payload":{"origin_node_id":9998,"event_id":17,"source":"local","packet_hex":"454d01020e270000110000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"}}';
  d jsonb:='{"record_id":"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:4","kind":"cancel","recorder_node_id":9998,"captured_at":null,"uptime_ms":40,"payload":{"origin_node_id":9998,"event_id":17,"source":"local","packet_hex":"454d01040e27000011000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000110000000e270000"}}';
  extra jsonb:=jsonb_set(b,'{record_id}','"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:5"');
  r jsonb;
BEGIN
  -- SQL is a trusted server adapter boundary; full payload validation is at HTTP handler.
  r:=public.study_sf_ingest(9998,jsonb_build_array(a,b,c,d));
  IF r<>jsonb_build_array(jsonb_build_object('record_id',a->>'record_id','status','accepted'),jsonb_build_object('record_id',b->>'record_id','status','accepted'),jsonb_build_object('record_id',c->>'record_id','status','accepted'),jsonb_build_object('record_id',d->>'record_id','status','accepted')) THEN RAISE EXCEPTION 'first confirmation mismatch'; END IF;
  r:=public.study_sf_ingest(9998,jsonb_build_array(a,b,c,d));
  IF r<>jsonb_build_array(jsonb_build_object('record_id',a->>'record_id','status','duplicate'),jsonb_build_object('record_id',b->>'record_id','status','duplicate'),jsonb_build_object('record_id',c->>'record_id','status','duplicate'),jsonb_build_object('record_id',d->>'record_id','status','duplicate')) THEN RAISE EXCEPTION 'replay mismatch'; END IF;
  BEGIN
    PERFORM public.study_sf_ingest(9998,jsonb_build_array(extra,jsonb_set(a,'{uptime_ms}','11')));
    RAISE EXCEPTION 'conflict should fail';
  EXCEPTION WHEN unique_violation THEN NULL; END;
  IF (SELECT count(*) FROM public.study_sf_records WHERE recorder_node_id=9998)<>4 THEN RAISE EXCEPTION 'atomic rollback failed'; END IF;
  BEGIN
    PERFORM public.study_sf_ingest(9998,jsonb_build_array(jsonb_set(a,'{recorder_node_id}','9997')));
    RAISE EXCEPTION 'recorder mismatch should fail' USING ERRCODE='XX000';
  EXCEPTION WHEN raise_exception THEN NULL; END;
  BEGIN
    PERFORM public.study_sf_ingest(9997,jsonb_build_array(jsonb_set(a,'{recorder_node_id}','9997')));
    RAISE EXCEPTION 'unregistered recorder should fail';
  EXCEPTION WHEN foreign_key_violation THEN NULL; END;
END $$;
RESET ROLE;
DO $$ BEGIN
 IF EXISTS(SELECT 1 FROM public.telemetry) OR EXISTS(SELECT 1 FROM public.emergency_alerts) OR EXISTS(SELECT 1 FROM public.sync_logs) THEN RAISE EXCEPTION 'legacy table changed'; END IF;
END $$;
ROLLBACK;
SELECT 'STUDY_SF SQL PASS: permissions, exact confirmations, duplicate replay, conflict rollback, recorder mismatch, foreign key' AS result;
