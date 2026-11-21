-- Local-only stand-in for essential Supabase roles/auth.users.
-- This file runs ONLY in the disposable postgres:17 Docker container.
CREATE ROLE anon NOLOGIN;
CREATE ROLE authenticated NOLOGIN;
CREATE ROLE service_role NOLOGIN BYPASSRLS;
CREATE SCHEMA auth;
CREATE TABLE auth.users (
  id uuid PRIMARY KEY
);
INSERT INTO auth.users(id) VALUES
  ('00000000-0000-4000-8000-000000000001'::uuid);
GRANT USAGE ON SCHEMA auth, public TO service_role;
GRANT SELECT ON auth.users TO service_role;
GRANT USAGE ON SCHEMA public TO anon, authenticated;
