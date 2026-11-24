-- REVIEW ONLY: apply to a separately approved disposable LOCAL lab, never production.
-- Existing five public tables and the F06 importer remain unchanged.
BEGIN;
CREATE TABLE public.study_sf_records (
  recorder_node_id bigint NOT NULL REFERENCES public.vessels(node_id) ON DELETE RESTRICT,
  record_id text NOT NULL CHECK(record_id ~ '^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}:[1-9][0-9]{0,9}$'),
  record_json jsonb NOT NULL CHECK(jsonb_typeof(record_json)='object' AND octet_length(record_json::text)<=2048),
  synced_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY(recorder_node_id,record_id),
  CHECK(record_json->>'record_id' IS NOT DISTINCT FROM record_id),
  CHECK(record_json->'recorder_node_id' IS NOT DISTINCT FROM to_jsonb(recorder_node_id)),
  CHECK(coalesce(record_json->>'kind' IN ('weather','gps','sos','cancel'),false))
);
ALTER TABLE public.study_sf_records ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON public.study_sf_records FROM PUBLIC, anon, authenticated, service_role;
GRANT SELECT,INSERT ON public.study_sf_records TO service_role;
-- Only the trusted server adapter can execute this function. Edge validates the full record schema.
CREATE FUNCTION public.study_sf_ingest(p_recorder bigint,p_records jsonb) RETURNS jsonb
LANGUAGE plpgsql SECURITY INVOKER SET search_path='' AS $$
DECLARE r jsonb; old jsonb; n integer; receipts jsonb:='[]'::jsonb; status text;
BEGIN
  IF p_recorder IS NULL OR p_recorder NOT BETWEEN 1 AND 9999 OR p_records IS NULL OR jsonb_typeof(p_records)<>'array' THEN
    RAISE EXCEPTION 'invalid batch';
  END IF;
  IF jsonb_array_length(p_records) NOT BETWEEN 1 AND 8 THEN RAISE EXCEPTION 'invalid batch size'; END IF;
  IF (SELECT count(DISTINCT e->>'record_id') FROM jsonb_array_elements(p_records) e) <> jsonb_array_length(p_records) THEN RAISE EXCEPTION 'invalid identities'; END IF;
  FOR r IN SELECT value FROM jsonb_array_elements(p_records) LOOP
    IF r->'recorder_node_id' IS DISTINCT FROM to_jsonb(p_recorder) THEN RAISE EXCEPTION 'recorder mismatch'; END IF;
    INSERT INTO public.study_sf_records(recorder_node_id,record_id,record_json)
      VALUES(p_recorder,r->>'record_id',r) ON CONFLICT (recorder_node_id,record_id) DO NOTHING;
    GET DIAGNOSTICS n=ROW_COUNT;
    IF n=1 THEN status:='accepted';
    ELSE
      SELECT record_json INTO old FROM public.study_sf_records WHERE recorder_node_id=p_recorder AND record_id=r->>'record_id';
      IF NOT FOUND OR old IS DISTINCT FROM r THEN RAISE EXCEPTION 'record identity conflict' USING ERRCODE='23505'; END IF;
      status:='duplicate';
    END IF;
    receipts:=receipts||jsonb_build_array(jsonb_build_object('record_id',r->>'record_id','status',status));
  END LOOP;
  RETURN receipts;
END $$;
REVOKE ALL ON FUNCTION public.study_sf_ingest(bigint,jsonb) FROM PUBLIC, anon, authenticated, service_role;
GRANT EXECUTE ON FUNCTION public.study_sf_ingest(bigint,jsonb) TO service_role;
COMMIT;
