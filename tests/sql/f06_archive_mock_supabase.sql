-- Local-only stand-in for essential Supabase roles/auth.users.
-- This file runs ONLY in the disposable postgres:17 Docker container.
CREATE ROLE anon NOLOGIN;
CREATE ROLE authenticated NOLOGIN;
CREATE ROLE service_role NOLOGIN BYPASSRLS;
CREATE SCHEMA auth;
CREATE TABLE auth.users (
  id uuid PRIMARY KEY,
  email text NOT NULL DEFAULT 'synthetic@example.invalid'
);
INSERT INTO auth.users(id) VALUES
  ('00000000-0000-4000-8000-000000000001'::uuid);
GRANT USAGE ON SCHEMA auth, public TO service_role;
-- No broad auth.users SELECT grant in the mock. The F06 proposal must
-- supply its narrowly scoped SELECT(id), matching the real local lab.
GRANT USAGE ON SCHEMA public TO anon, authenticated;
