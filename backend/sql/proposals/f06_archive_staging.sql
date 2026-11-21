-- REVIEW ONLY. Historical F06 SOS archival staging, NOT active emergency ingestion.
-- NOT APPLIED to the live Supabase project. Generate a proper migration with
-- 'supabase migration new ...' after security/access review.
-- Existing five public tables and their current privileges are untouched.
BEGIN;

-- Independent export approval. Never populated by the importer.
CREATE TABLE public.dtn_f06_approved_exports (
  vessel_number integer NOT NULL CHECK (vessel_number BETWEEN 1 AND 9999),
  session_name text NOT NULL CHECK (session_name ~ '^S[0-9A-F]{7}$'),
  journal_name text NOT NULL DEFAULT 'sos_events.jsonl'
    CHECK (journal_name = 'sos_events.jsonl'),
  file_sha256 text NOT NULL CHECK (file_sha256 ~ '^[0-9A-F]{64}$'),
  original_bytes integer NOT NULL CHECK (original_bytes BETWEEN 0 AND 65536),
  record_count integer NOT NULL CHECK (record_count BETWEEN 0 AND 128),
  approved_by uuid NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
  approved_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (vessel_number, session_name, journal_name)
);

CREATE TABLE public.dtn_f06_history_files (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  imported_by uuid NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
  vessel_number integer NOT NULL CHECK (vessel_number BETWEEN 1 AND 9999),
  session_name text NOT NULL CHECK (session_name ~ '^S[0-9A-F]{7}$'),
  journal_name text NOT NULL DEFAULT 'sos_events.jsonl'
    CHECK (journal_name = 'sos_events.jsonl'),
  file_sha256 text NOT NULL CHECK (file_sha256 ~ '^[0-9A-F]{64}$'),
  original_bytes integer NOT NULL CHECK (original_bytes BETWEEN 0 AND 65536),
  record_count integer NOT NULL CHECK (record_count BETWEEN 0 AND 128),
  archived_at timestamptz NOT NULL DEFAULT now(),
  UNIQUE (vessel_number, session_name, journal_name)
);

CREATE TABLE public.dtn_f06_history_records (
  file_id uuid NOT NULL REFERENCES public.dtn_f06_history_files(id) ON DELETE RESTRICT,
  vessel_number integer NOT NULL CHECK (vessel_number BETWEEN 1 AND 9999),
  ordinal integer NOT NULL CHECK (ordinal BETWEEN 1 AND 128),
  boot_token bigint NOT NULL CHECK (boot_token BETWEEN 0 AND 4294967295),
  event_index bigint NOT NULL CHECK (event_index BETWEEN 1 AND 4294967295),
  raw_line text NOT NULL CHECK (octet_length(raw_line) BETWEEN 2 AND 1024),
  record_json jsonb NOT NULL CHECK (jsonb_typeof(record_json) = 'object'),
  archived_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (file_id, ordinal),
  UNIQUE (vessel_number, boot_token, event_index)
);

-- These are restricted archival evidence tables, not dashboard Data API tables.
ALTER TABLE public.dtn_f06_approved_exports ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.dtn_f06_history_files ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.dtn_f06_history_records ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON public.dtn_f06_approved_exports,
  public.dtn_f06_history_files, public.dtn_f06_history_records
  FROM PUBLIC, anon, authenticated;
REVOKE INSERT, UPDATE, DELETE ON public.dtn_f06_approved_exports FROM service_role;
GRANT SELECT ON public.dtn_f06_approved_exports TO service_role;
GRANT SELECT, INSERT ON public.dtn_f06_history_files,
  public.dtn_f06_history_records TO service_role;
-- No RLS policies: neither anon nor ordinary logged-in users can read/write.

CREATE FUNCTION public.dtn_archive_f06_sos_file(
  p_actor uuid, p_vessel integer, p_session text, p_sha256 text,
  p_size integer, p_records jsonb
) RETURNS jsonb
LANGUAGE plpgsql
SECURITY INVOKER
SET search_path = ''
AS $$
DECLARE
  v_file uuid;
  v_existing public.dtn_f06_history_files%ROWTYPE;
  v_record jsonb;
  v_n integer := 0;
  v_count integer;
  v_boot bigint;
  v_index bigint;
  v_line text;
BEGIN
  IF p_actor IS NULL OR NOT EXISTS (
    SELECT 1 FROM auth.users WHERE id = p_actor
  ) THEN
    RAISE EXCEPTION 'registered archive operator required';
  END IF;
  IF p_vessel IS NULL OR p_vessel NOT BETWEEN 1 AND 9999
    OR p_session IS NULL OR p_session !~ '^S[0-9A-F]{7}$'
    OR p_sha256 IS NULL OR p_sha256 !~ '^[0-9A-F]{64}$'
    OR p_size IS NULL OR p_size NOT BETWEEN 0 AND 65536
    OR p_records IS NULL OR jsonb_typeof(p_records) <> 'array'
  THEN
    RAISE EXCEPTION 'invalid archive envelope';
  END IF;
  v_count := jsonb_array_length(p_records);
  IF v_count > 128 THEN
    RAISE EXCEPTION 'archive batch too large';
  END IF;

  -- A self-declared file digest never grants archive authorization.
  IF NOT EXISTS (
    SELECT 1 FROM public.dtn_f06_approved_exports a
    WHERE a.vessel_number = p_vessel
      AND a.session_name = p_session
      AND a.journal_name = 'sos_events.jsonl'
      AND a.file_sha256 = p_sha256
      AND a.original_bytes = p_size
      AND a.record_count = v_count
  ) THEN
    RAISE EXCEPTION 'closed-export manifest not independently approved';
  END IF;

  -- Unique session/journal acts as an immutable file idempotency key.
  INSERT INTO public.dtn_f06_history_files
    (imported_by,vessel_number,session_name,file_sha256,original_bytes,record_count)
  VALUES (p_actor,p_vessel,p_session,p_sha256,p_size,v_count)
  ON CONFLICT (vessel_number,session_name,journal_name) DO NOTHING
  RETURNING id INTO v_file;

  IF v_file IS NULL THEN
    SELECT * INTO v_existing FROM public.dtn_f06_history_files
      WHERE vessel_number = p_vessel AND session_name = p_session
        AND journal_name = 'sos_events.jsonl'
      FOR UPDATE;
    IF NOT FOUND OR v_existing.file_sha256 <> p_sha256
       OR v_existing.original_bytes <> p_size
       OR v_existing.record_count <> v_count THEN
      RAISE EXCEPTION 'immutable archive session conflict';
    END IF;
    RETURN jsonb_build_object('status','duplicate','record_count',v_count,
      'scope','historical_sos_archive_only','emergency_alerts_changed',false);
  END IF;

  FOR v_record IN SELECT value FROM jsonb_array_elements(p_records)
  LOOP
    v_n := v_n + 1;
    IF jsonb_typeof(v_record) <> 'object'
       OR (v_record->>'ordinal')::integer <> v_n
       OR jsonb_typeof(v_record->'body') <> 'object'
       OR (v_record->'body'->>'vessel_id')::integer <> p_vessel
       OR (v_record->'body'->>'kind') NOT IN
       ('sos_rx','sos_ack_tx','sos_ack_rx','sos_local_start',
        'sos_local_cancel','sos_tx_attempt','cancel_rx','cancel_ack_rx',
        'cancel_ack_tx','cancel_tx_attempt','relay_tx') THEN
      RAISE EXCEPTION 'invalid archived SOS record';
    END IF;
    v_line := v_record->>'raw_line';
    v_boot := (v_record->>'boot_token')::bigint;
    v_index := (v_record->>'event_index')::bigint;
    IF v_line IS NULL OR octet_length(v_line) NOT BETWEEN 2 AND 1024
       OR v_boot IS NULL OR v_boot NOT BETWEEN 0 AND 4294967295
       OR v_index IS NULL OR v_index NOT BETWEEN 1 AND 4294967295
       OR (v_record->'body'->>'boot_token')::bigint <> v_boot
       OR (v_record->'body'->>'event_index')::bigint <> v_index THEN
      RAISE EXCEPTION 'invalid archived event identity';
    END IF;
    INSERT INTO public.dtn_f06_history_records
      (file_id,vessel_number,ordinal,boot_token,event_index,raw_line,record_json)
    VALUES (v_file,p_vessel,v_n,v_boot,v_index,v_line,v_record->'body');
  END LOOP;

  RETURN jsonb_build_object('status','accepted','record_count',v_count,
    'scope','historical_sos_archive_only','emergency_alerts_changed',false);
END
$$;

REVOKE ALL ON FUNCTION public.dtn_archive_f06_sos_file(
  uuid,integer,text,text,integer,jsonb) FROM PUBLIC, anon, authenticated;
GRANT EXECUTE ON FUNCTION public.dtn_archive_f06_sos_file(
  uuid,integer,text,text,integer,jsonb) TO service_role;

COMMIT;
