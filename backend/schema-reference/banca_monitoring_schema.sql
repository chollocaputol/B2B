--
-- PostgreSQL database dump
--

\restrict Qf2aSvcYXwu6nKHhaTeCe9k3ul8WXjI7VdMVhqLu7BTKtf6OdCCYwBSPa1zUhGp

-- Dumped from database version 18.6
-- Dumped by pg_dump version 18.6

-- Started on 2026-10-24 15:36:53

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET transaction_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- TOC entry 222 (class 1259 OID 16466)
-- Name: communication_tests; Type: TABLE; Schema: public; Owner: -
--

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
    CONSTRAINT communication_tests_distance_m_check CHECK ((distance_m >= (0)::double precision)),
    CONSTRAINT communication_tests_packets_received_check CHECK ((packets_received >= 0)),
    CONSTRAINT communication_tests_pdr_percent_check CHECK (((pdr_percent >= (0)::double precision) AND (pdr_percent <= (100)::double precision)))
);


--
-- TOC entry 221 (class 1259 OID 16437)
-- Name: emergency_alerts; Type: TABLE; Schema: public; Owner: -
--

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
    CONSTRAINT emergency_alerts_battery_percent_check CHECK (((battery_percent >= 0) AND (battery_percent <= 100))),
    CONSTRAINT emergency_alerts_weather_state_check CHECK ((weather_state = ANY (ARRAY[0, 1, 2, 3]))),
    CONSTRAINT emergency_latitude_range CHECK (((latitude IS NULL) OR ((latitude >= ('-90'::integer)::double precision) AND (latitude <= (90)::double precision)))),
    CONSTRAINT emergency_longitude_range CHECK (((longitude IS NULL) OR ((longitude >= ('-180'::integer)::double precision) AND (longitude <= (180)::double precision))))
);


--
-- TOC entry 223 (class 1259 OID 16491)
-- Name: sync_logs; Type: TABLE; Schema: public; Owner: -
--

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
    CONSTRAINT sync_logs_records_failed_check CHECK ((records_failed >= 0)),
    CONSTRAINT sync_logs_sync_status_check CHECK ((sync_status = ANY (ARRAY[('PENDING'::character varying)::text, ('IN_PROGRESS'::character varying)::text, ('SUCCESS'::character varying)::text, ('PARTIAL'::character varying)::text, ('FAILED'::character varying)::text])))
);


--
-- TOC entry 220 (class 1259 OID 16405)
-- Name: telemetry; Type: TABLE; Schema: public; Owner: -
--

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
    CONSTRAINT telemetry_battery_percent_check CHECK (((battery_percent >= 0) AND (battery_percent <= 100))),
    CONSTRAINT telemetry_gps_coordinates CHECK (((gps_fix = false) OR ((latitude IS NOT NULL) AND (longitude IS NOT NULL)))),
    CONSTRAINT telemetry_hop_count_check CHECK ((hop_count >= 0)),
    CONSTRAINT telemetry_latitude_range CHECK (((latitude IS NULL) OR ((latitude >= ('-90'::integer)::double precision) AND (latitude <= (90)::double precision)))),
    CONSTRAINT telemetry_longitude_range CHECK (((longitude IS NULL) OR ((longitude >= ('-180'::integer)::double precision) AND (longitude <= (180)::double precision)))),
    CONSTRAINT telemetry_weather_state_check CHECK ((weather_state = ANY (ARRAY[0, 1, 2, 3])))
);


--
-- TOC entry 219 (class 1259 OID 16391)
-- Name: vessels; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.vessels (
    id uuid DEFAULT gen_random_uuid() NOT NULL,
    registration_serial_no text NOT NULL,
    vessel_name text NOT NULL,
    node_id bigint NOT NULL
);


--
-- TOC entry 5072 (class 0 OID 16466)
-- Dependencies: 222
-- Data for Name: communication_tests; Type: TABLE DATA; Schema: public; Owner: -
--

COPY public.communication_tests (id, distance_m, packets_received, pdr_percent, notes, transmitter_node_id, receiver_node_id, test_started_at, test_ended_at, line_of_sight, packets_generated, packets_missing, duplicate_packets, avg_rssi_dbm, min_rssi_dbm, max_rssi_dbm, avg_snr_db, min_snr_db, max_snr_db) FROM stdin;
\.


--
-- TOC entry 5071 (class 0 OID 16437)
-- Dependencies: 221
-- Data for Name: emergency_alerts; Type: TABLE DATA; Schema: public; Owner: -
--

COPY public.emergency_alerts (id, vessel_id, origin_node_id, latitude, longitude, battery_percent, weather_state, received_at, resolved_at, triggered_at, resolved) FROM stdin;
\.


--
-- TOC entry 5073 (class 0 OID 16491)
-- Dependencies: 223
-- Data for Name: sync_logs; Type: TABLE DATA; Schema: public; Owner: -
--

COPY public.sync_logs (id, vessel_id, sync_status, records_failed, sync_started_at, sync_completed_at, records_queued, records_sent, records_successful, duplicates_detected, data_loss_count, error_message) FROM stdin;
\.


--
-- TOC entry 5070 (class 0 OID 16405)
-- Dependencies: 220
-- Data for Name: telemetry; Type: TABLE DATA; Schema: public; Owner: -
--

COPY public.telemetry (id, vessel_id, origin_node_id, sequence_number, device_timestamp_s, received_at, synced_at, gps_fix, latitude, longitude, shore_distance_m, temperature_c, humidity_percent, pressure_hpa, pressure_trend_hpa, zambretti_index, weather_state, battery_percent, rssi_dbm, snr_db, hop_count) FROM stdin;
\.


--
-- TOC entry 5069 (class 0 OID 16391)
-- Dependencies: 219
-- Data for Name: vessels; Type: TABLE DATA; Schema: public; Owner: -
--

COPY public.vessels (id, registration_serial_no, vessel_name, node_id) FROM stdin;
\.


--
-- TOC entry 4916 (class 2606 OID 16757)
-- Name: communication_tests communication_tests_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.communication_tests
    ADD CONSTRAINT communication_tests_pkey PRIMARY KEY (id);


--
-- TOC entry 4912 (class 2606 OID 16755)
-- Name: emergency_alerts emergency_alerts_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.emergency_alerts
    ADD CONSTRAINT emergency_alerts_pkey PRIMARY KEY (id);


--
-- TOC entry 4918 (class 2606 OID 16759)
-- Name: sync_logs sync_logs_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.sync_logs
    ADD CONSTRAINT sync_logs_pkey PRIMARY KEY (id);


--
-- TOC entry 4910 (class 2606 OID 16753)
-- Name: telemetry telemetry_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.telemetry
    ADD CONSTRAINT telemetry_pkey PRIMARY KEY (id);


--
-- TOC entry 4900 (class 2606 OID 16784)
-- Name: vessels vessels_node_id_key; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.vessels
    ADD CONSTRAINT vessels_node_id_key UNIQUE (node_id);


--
-- TOC entry 4902 (class 2606 OID 16751)
-- Name: vessels vessels_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.vessels
    ADD CONSTRAINT vessels_pkey PRIMARY KEY (id);


--
-- TOC entry 4904 (class 2606 OID 16782)
-- Name: vessels vessels_registration_serial_no_key; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.vessels
    ADD CONSTRAINT vessels_registration_serial_no_key UNIQUE (registration_serial_no);


--
-- TOC entry 4913 (class 1259 OID 16462)
-- Name: idx_emergency_received; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_emergency_received ON public.emergency_alerts USING btree (received_at DESC);


--
-- TOC entry 4914 (class 1259 OID 16726)
-- Name: idx_emergency_vessel; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_emergency_vessel ON public.emergency_alerts USING btree (vessel_id, received_at DESC);


--
-- TOC entry 4905 (class 1259 OID 16798)
-- Name: idx_telemetry_node_sequence; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_telemetry_node_sequence ON public.telemetry USING btree (origin_node_id, sequence_number);


--
-- TOC entry 4906 (class 1259 OID 16433)
-- Name: idx_telemetry_received; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_telemetry_received ON public.telemetry USING btree (received_at DESC);


--
-- TOC entry 4907 (class 1259 OID 16717)
-- Name: idx_telemetry_vessel_received; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_telemetry_vessel_received ON public.telemetry USING btree (vessel_id, received_at DESC);


--
-- TOC entry 4908 (class 1259 OID 16434)
-- Name: idx_telemetry_weather; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_telemetry_weather ON public.telemetry USING btree (weather_state, received_at DESC);


--
-- TOC entry 4920 (class 2606 OID 16765)
-- Name: emergency_alerts emergency_alerts_vessel_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.emergency_alerts
    ADD CONSTRAINT emergency_alerts_vessel_id_fkey FOREIGN KEY (vessel_id) REFERENCES public.vessels(id) ON DELETE RESTRICT;


--
-- TOC entry 4921 (class 2606 OID 16775)
-- Name: sync_logs sync_logs_vessel_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.sync_logs
    ADD CONSTRAINT sync_logs_vessel_id_fkey FOREIGN KEY (vessel_id) REFERENCES public.vessels(id) ON DELETE RESTRICT;


--
-- TOC entry 4919 (class 2606 OID 16760)
-- Name: telemetry telemetry_vessel_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.telemetry
    ADD CONSTRAINT telemetry_vessel_id_fkey FOREIGN KEY (vessel_id) REFERENCES public.vessels(id) ON DELETE RESTRICT;


-- Completed on 2026-10-24 15:36:54

--
-- PostgreSQL database dump complete
--

\unrestrict Qf2aSvcYXwu6nKHhaTeCe9k3ul8WXjI7VdMVhqLu7BTKtf6OdCCYwBSPa1zUhGp

