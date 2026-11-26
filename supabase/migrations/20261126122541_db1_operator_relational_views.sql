-- DB1: operator read model; preserve DTN upload contract and raw evidence.
-- Existing reviewer claim authorizes the single operator interface.
CREATE POLICY operator_vessel_read ON public.vessels FOR SELECT TO authenticated
USING ((SELECT auth.jwt()->'app_metadata'->'thesis_study_reviewer') = 'true'::jsonb
   AND (SELECT auth.jwt()->'app_metadata'->'thesis_study_device') IS DISTINCT FROM 'true'::jsonb);
GRANT SELECT (id,node_id,registration_serial_no,vessel_name) ON public.vessels TO authenticated;

CREATE VIEW public.synchronization_history WITH (security_invoker=true) AS
SELECT v.id AS vessel_id, r.recorder_node_id, r.record_id,
       r.record_json->>'kind' AS kind,
       (r.record_json->>'captured_at')::timestamptz AS captured_at,
       (r.record_json->>'uptime_ms')::bigint AS uptime_ms, r.synced_at
FROM public.study_sf_records r
JOIN public.vessels v ON v.node_id=r.recorder_node_id
WHERE NOT (r.recorder_node_id=3 AND (
  r.record_id ~ '^aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:([1-9]|[1-3][0-9]|40)$'
  OR r.record_id ~ '^bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb:[1-8]$'));

CREATE VIEW public.vessel_telemetry WITH (security_invoker=true) AS
SELECT h.vessel_id,h.recorder_node_id,h.record_id,h.captured_at,h.uptime_ms,h.synced_at,
       (r.record_json#>>'{payload,gps_fix}')::boolean AS gps_fix,
       (r.record_json#>>'{payload,latitude}')::double precision AS latitude,
       (r.record_json#>>'{payload,longitude}')::double precision AS longitude
FROM public.synchronization_history h JOIN public.study_sf_records r
USING (recorder_node_id,record_id) WHERE h.kind='gps';

CREATE VIEW public.weather_observations WITH (security_invoker=true) AS
SELECT h.vessel_id,h.recorder_node_id,h.record_id,h.captured_at,h.uptime_ms,h.synced_at,
       (r.record_json#>>'{payload,temperature_c}')::double precision AS temperature_c,
       (r.record_json#>>'{payload,humidity_percent}')::double precision AS humidity_percent,
       (r.record_json#>>'{payload,pressure_hpa}')::double precision AS pressure_hpa,
       (r.record_json#>>'{payload,pressure_trend_hpa}')::double precision AS pressure_trend_hpa,
       (r.record_json#>>'{payload,weather_state}')::smallint AS weather_state,
       (r.record_json#>>'{payload,micro,input_valid}')::boolean AS micro_input_valid,
       r.record_json#>>'{payload,micro,state}' AS micro_state,
       r.record_json#>>'{payload,micro,pattern}' AS micro_pattern
FROM public.synchronization_history h JOIN public.study_sf_records r
USING (recorder_node_id,record_id) WHERE h.kind='weather';

CREATE VIEW public.sos_observations WITH (security_invoker=true) AS
SELECT h.vessel_id AS recorder_vessel_id,h.recorder_node_id,h.record_id,h.kind,
       h.captured_at,h.uptime_ms,h.synced_at,
       (r.record_json#>>'{payload,origin_node_id}')::bigint AS origin_node_id,
       (r.record_json#>>'{payload,event_id}')::bigint AS event_id,
       r.record_json#>>'{payload,source}' AS source,
       r.record_json#>>'{payload,packet_hex}' AS packet_hex
FROM public.synchronization_history h JOIN public.study_sf_records r
USING (recorder_node_id,record_id) WHERE h.kind IN ('sos','cancel');

CREATE VIEW public.sos_events WITH (security_invoker=true) AS
SELECT o.origin_node_id,o.event_id,v.id AS origin_vessel_id,
       count(*) FILTER (WHERE o.kind='sos') AS sos_observation_count,
       count(*) FILTER (WHERE o.kind='cancel') AS cancel_observation_count,
       count(DISTINCT o.recorder_node_id) AS recording_node_count,
       min(o.captured_at) FILTER (WHERE o.kind='sos') AS first_known_sos_capture_at,
       min(o.captured_at) FILTER (WHERE o.kind='cancel') AS first_known_cancel_capture_at,
       min(o.synced_at) AS first_receipt_at,max(o.synced_at) AS last_receipt_at,
       CASE WHEN count(*) FILTER (WHERE o.kind='cancel')>0 THEN 'Cancellation observed'
            ELSE 'SOS observed; no cancellation recorded' END AS observation_state
FROM public.sos_observations o LEFT JOIN public.vessels v ON v.node_id=o.origin_node_id
GROUP BY o.origin_node_id,o.event_id,v.id;

REVOKE ALL ON public.synchronization_history,public.vessel_telemetry,
public.weather_observations,public.sos_observations,public.sos_events
FROM PUBLIC,anon,authenticated,service_role;
GRANT SELECT ON public.synchronization_history,public.vessel_telemetry,
public.weather_observations,public.sos_observations,public.sos_events TO authenticated;

-- Structural verification inside migration; any failure rolls back.
DO $verify$
DECLARE expected bigint; actual bigint;
BEGIN
  SELECT count(*) INTO expected FROM public.study_sf_records r
  WHERE NOT (r.recorder_node_id=3 AND (
    r.record_id ~ '^aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa:([1-9]|[1-3][0-9]|40)$'
    OR r.record_id ~ '^bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb:[1-8]$'));
  SELECT count(*) INTO actual FROM public.synchronization_history;
  IF expected<>actual OR actual<>(SELECT count(*) FROM public.vessel_telemetry)+(SELECT count(*) FROM public.weather_observations)+(SELECT count(*) FROM public.sos_observations) THEN
    RAISE EXCEPTION 'DB1 observation partition mismatch';
  END IF;
END $verify$;
