# Operator dashboard

The operator dashboard is a React/TypeScript application built with Vite. It displays synchronized vessel observations, emergency history and the macro and micro weather layers. Unknown values and missing capture times are not replaced by invented live measurements.

## Local setup

Install Node.js and pnpm compatible with the retained project configuration and lockfile. From `frontend/`:

```sh
pnpm install --frozen-lockfile
pnpm run dev
pnpm run build
```

Use the supplied `.env.example` as a template for private local configuration. Set `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY` to the intended hosted project. These frontend variables are visible to the browser: never place passwords, service-role keys or secret keys in them.

Use an authorized operator account for access to actual synchronized records. The separate demonstration interface is illustrative and must not be treated as field observations.

## Deployment

The root `netlify.toml` defines the frontend build and publish settings. Preserve the existing hosted environment variables, authentication and backend contract. A frontend build alone does not require a database migration, reset or Edge Function redeployment.

The published tree contains the application and its dependency lockfile. Regression tests and historical development reports are retained in the private local development archive.
