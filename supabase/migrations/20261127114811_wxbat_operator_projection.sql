-- WXBAT2C draft: append-only projection columns; no table, grant, policy or RPC changes.
CREATE OR REPLACE VIEW public.vessel_telemetry WITH (security_invoker=true) AS
SELECT h.vessel_id,h.recorder_node_id,h.record_id,h.captured_at,h.uptime_ms,h.synced_at,
       (r.record_json#>>'{payload,gps_fix}')::boolean AS gps_fix,
       (r.record_json#>>'{payload,latitude}')::double precision AS latitude,
       (r.record_json#>>'{payload,longitude}')::double precision AS longitude,
       CASE WHEN jsonb_typeof(r.record_json#>'{payload,battery_percent}')='number' THEN
         CASE WHEN (r.record_json#>>'{payload,battery_percent}')::numeric BETWEEN 0 AND 100
          AND trunc((r.record_json#>>'{payload,battery_percent}')::numeric)=(r.record_json#>>'{payload,battery_percent}')::numeric
         THEN (r.record_json#>>'{payload,battery_percent}')::numeric::smallint END END AS battery_percent
FROM public.synchronization_history h JOIN public.study_sf_records r
USING (recorder_node_id,record_id) WHERE h.kind='gps';

CREATE OR REPLACE VIEW public.weather_observations WITH (security_invoker=true) AS
SELECT h.vessel_id,h.recorder_node_id,h.record_id,h.captured_at,h.uptime_ms,h.synced_at,
       (r.record_json#>>'{payload,temperature_c}')::double precision AS temperature_c,
       (r.record_json#>>'{payload,humidity_percent}')::double precision AS humidity_percent,
       (r.record_json#>>'{payload,pressure_hpa}')::double precision AS pressure_hpa,
       (r.record_json#>>'{payload,pressure_trend_hpa}')::double precision AS pressure_trend_hpa,
       (r.record_json#>>'{payload,weather_state}')::smallint AS weather_state,
       (r.record_json#>>'{payload,micro,input_valid}')::boolean AS micro_input_valid,
       r.record_json#>>'{payload,micro,state}' AS micro_state,
       r.record_json#>>'{payload,micro,pattern}' AS micro_pattern,
       CASE WHEN jsonb_typeof(r.record_json#>'{payload,macro,input_valid}')='boolean' THEN (r.record_json#>>'{payload,macro,input_valid}')::boolean END AS macro_input_valid,
       CASE WHEN jsonb_typeof(r.record_json#>'{payload,macro,history_count}')='number' THEN
         CASE WHEN (r.record_json#>>'{payload,macro,history_count}')::numeric BETWEEN 0 AND 10 AND trunc((r.record_json#>>'{payload,macro,history_count}')::numeric)=(r.record_json#>>'{payload,macro,history_count}')::numeric THEN (r.record_json#>>'{payload,macro,history_count}')::numeric::smallint END END AS macro_history_count,
       CASE WHEN jsonb_typeof(r.record_json#>'{payload,macro,state}')='number' THEN
         CASE WHEN (r.record_json#>>'{payload,macro,state}')::numeric BETWEEN 0 AND 3 AND trunc((r.record_json#>>'{payload,macro,state}')::numeric)=(r.record_json#>>'{payload,macro,state}')::numeric THEN (r.record_json#>>'{payload,macro,state}')::numeric::smallint END END AS macro_state,
       CASE WHEN jsonb_typeof(r.record_json#>'{payload,macro,zambretti_index}')='number' THEN
         CASE WHEN (r.record_json#>>'{payload,macro,zambretti_index}')::numeric BETWEEN 0 AND 255 AND trunc((r.record_json#>>'{payload,macro,zambretti_index}')::numeric)=(r.record_json#>>'{payload,macro,zambretti_index}')::numeric THEN (r.record_json#>>'{payload,macro,zambretti_index}')::numeric::smallint END END AS macro_zambretti_index
FROM public.synchronization_history h JOIN public.study_sf_records r
USING (recorder_node_id,record_id) WHERE h.kind='weather';
