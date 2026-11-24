-- Local study review only; apply after study_sf_ingest.sql in isolated lab.
BEGIN;
GRANT SELECT ON public.study_sf_records TO authenticated;
CREATE POLICY study_sf_reviewer_read ON public.study_sf_records FOR SELECT TO authenticated
USING ((SELECT auth.jwt()->'app_metadata'->'thesis_study_reviewer') = 'true'::jsonb
       AND (SELECT auth.jwt()->'app_metadata'->'thesis_study_device') IS DISTINCT FROM 'true'::jsonb);
COMMIT;
