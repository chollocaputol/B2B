-- Initial schema for the Banca Monitoring System.

CREATE TABLE public.vessels (
    id uuid DEFAULT gen_random_uuid() NOT NULL,
    registration_serial_no text NOT NULL,
    vessel_name text NOT NULL,
    node_id bigint NOT NULL,
    CONSTRAINT vessels_pkey PRIMARY KEY (id),
    CONSTRAINT vessels_node_id_key UNIQUE (node_id),
    CONSTRAINT vessels_registration_serial_no_key UNIQUE (registration_serial_no)
);

CREATE TABLE public.telemetry (
    id uuid DEFAULT gen_random_uuid() NOT NULL,
    vessel_id uuid NOT NULL,
    origin_node_id bigint NOT NULL,
    sequence_number bigint NOT NULL,
    device_timestamp_s bigint NOT NULL,
    received_at timestamp with time zone,
    synced_at timestamp with time zone DEFAULT now(),
    gps_fix boolean NOT NULL,
    latitude double precision,
    longitude double precision,
    shore_distance_m integer,
    temperature_c real,
    humidity_percent real,
    pressure_hpa real,
    pressure_trend_hpa real,
    zambretti_index smallint,
    weather_state smallint NOT NULL,
    battery_percent smallint,
    rssi_dbm real,
    snr_db real,
    hop_count smallint,
    CONSTRAINT telemetry_pkey PRIMARY KEY (id),
    CONSTRAINT telemetry_vessel_id_fkey FOREIGN KEY (vessel_id)
        REFERENCES public.vessels(id) ON DELETE RESTRICT,
    CONSTRAINT telemetry_battery_percent_check
        CHECK (battery_percent >= 0 AND battery_percent <= 100),
    CONSTRAINT telemetry_gps_coordinates
        CHECK (gps_fix = false OR (latitude IS NOT NULL AND longitude IS NOT NULL)),
    CONSTRAINT telemetry_hop_count_check CHECK (hop_count >= 0),
    CONSTRAINT telemetry_latitude_range
        CHECK (latitude IS NULL OR (latitude >= -90 AND latitude <= 90)),
    CONSTRAINT telemetry_longitude_range
        CHECK (longitude IS NULL OR (longitude >= -180 AND longitude <= 180)),
    CONSTRAINT telemetry_weather_state_check CHECK (weather_state IN (0, 1, 2, 3))
);

CREATE TABLE public.emergency_alerts (
    id uuid DEFAULT gen_random_uuid() NOT NULL,
    vessel_id uuid NOT NULL,
    origin_node_id bigint CONSTRAINT emergency_alerts_node_id_not_null NOT NULL,
    latitude double precision,
    longitude double precision,
    battery_percent smallint,
    weather_state smallint NOT NULL,
    received_at timestamp with time zone DEFAULT now() NOT NULL,
    resolved_at timestamp with time zone,
    triggered_at timestamp with time zone NOT NULL,
    resolved boolean DEFAULT false NOT NULL,
    CONSTRAINT emergency_alerts_pkey PRIMARY KEY (id),
    CONSTRAINT emergency_alerts_vessel_id_fkey FOREIGN KEY (vessel_id)
        REFERENCES public.vessels(id) ON DELETE RESTRICT,
    CONSTRAINT emergency_alerts_battery_percent_check
        CHECK (battery_percent >= 0 AND battery_percent <= 100),
    CONSTRAINT emergency_alerts_weather_state_check CHECK (weather_state IN (0, 1, 2, 3)),
    CONSTRAINT emergency_latitude_range
        CHECK (latitude IS NULL OR (latitude >= -90 AND latitude <= 90)),
    CONSTRAINT emergency_longitude_range
        CHECK (longitude IS NULL OR (longitude >= -180 AND longitude <= 180))
);

CREATE TABLE public.communication_tests (
    id uuid DEFAULT gen_random_uuid() NOT NULL,
    distance_m double precision,
    packets_received integer,
    pdr_percent real,
    notes text,
    transmitter_node_id bigint,
    receiver_node_id bigint,
    test_started_at timestamp with time zone,
    test_ended_at timestamp with time zone,
    line_of_sight boolean,
    packets_generated integer,
    packets_missing integer,
    duplicate_packets integer DEFAULT 0 NOT NULL,
    avg_rssi_dbm real,
    min_rssi_dbm real,
    max_rssi_dbm real,
    avg_snr_db real,
    min_snr_db real,
    max_snr_db real,
    CONSTRAINT communication_tests_pkey PRIMARY KEY (id),
    CONSTRAINT communication_tests_distance_m_check CHECK (distance_m >= 0),
    CONSTRAINT communication_tests_packets_received_check CHECK (packets_received >= 0),
    CONSTRAINT communication_tests_pdr_percent_check CHECK (pdr_percent >= 0 AND pdr_percent <= 100)
);

CREATE TABLE public.sync_logs (
    id uuid DEFAULT gen_random_uuid() NOT NULL,
    vessel_id uuid,
    sync_status text DEFAULT 'PENDING'::character varying NOT NULL,
    records_failed integer DEFAULT 0 NOT NULL,
    sync_started_at timestamp with time zone,
    sync_completed_at timestamp with time zone,
    records_queued integer,
    records_sent integer,
    records_successful integer,
    duplicates_detected integer DEFAULT 0 NOT NULL,
    data_loss_count integer DEFAULT 0 NOT NULL,
    error_message text,
    CONSTRAINT sync_logs_pkey PRIMARY KEY (id),
    CONSTRAINT sync_logs_vessel_id_fkey FOREIGN KEY (vessel_id)
        REFERENCES public.vessels(id) ON DELETE RESTRICT,
    CONSTRAINT sync_logs_records_failed_check CHECK (records_failed >= 0),
    CONSTRAINT sync_logs_sync_status_check
        CHECK (sync_status IN ('PENDING', 'IN_PROGRESS', 'SUCCESS', 'PARTIAL', 'FAILED'))
);

CREATE INDEX idx_emergency_received
    ON public.emergency_alerts USING btree (received_at DESC);
CREATE INDEX idx_emergency_vessel
    ON public.emergency_alerts USING btree (vessel_id, received_at DESC);
CREATE INDEX idx_telemetry_node_sequence
    ON public.telemetry USING btree (origin_node_id, sequence_number);
CREATE INDEX idx_telemetry_received
    ON public.telemetry USING btree (received_at DESC);
CREATE INDEX idx_telemetry_vessel_received
    ON public.telemetry USING btree (vessel_id, received_at DESC);
CREATE INDEX idx_telemetry_weather
    ON public.telemetry USING btree (weather_state, received_at DESC);
