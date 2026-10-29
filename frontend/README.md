# Acmac fisheries dashboard

Public React + TypeScript dashboard for small-scale fishermen and shore monitoring in Barangay Acmac, Iligan City, Lanao del Norte. No login, credentials, or Supabase project is required. All vessel names, positions, telemetry and SOS events are fictional demonstration data. The demo clock is captured at page load.

## Local development

Install Node.js 24, then from `frontend/`:

```sh
npm install
npm run dev
```

Open the URL printed by Vite. Alternatively use pnpm: `pnpm install` and `pnpm dev`. A pnpm lockfile is included for reproducible installation.

```sh
npm test
npm run typecheck
npm run build
npm run preview
```

## Deploy to Netlify

1. Push this repository to your Git provider.
2. In Netlify, import the repository. The root `netlify.toml` configures the base directory `frontend`, build command `npm run build`, output `dist`, and Node 24.
3. Deploy and open the assigned Netlify URL. No environment variables are needed.

Netlify should detect the pnpm lockfile and install dependencies using pnpm. The build script itself works with either npm or pnpm. For a manual deployment, build locally and upload `frontend/dist` to Netlify.

## Interface

Select a vessel from the dropdown, node list, or map. Switch between 24 hours and seven days, choose temperature, humidity, air pressure or LoRa RSSI, search telemetry and paginate. Export downloads the full set of readings matching the selected vessel, range and search, rather than only the current table page. Navigation links jump to sections on the same page.

“Online” refers to a packet within 15 minutes of the captured demo clock; this is a demo status rule, not a firmware timeout. Demo vessel C stopped reporting over three hours earlier and has no valid GPS fix. All charts, event times and tables use Philippine Standard Time (Asia/Manila, UTC+8). Reload to capture a new snapshot.

Vessel A models a GPS/weather sensor node; Vessel B models a GPS/LoRa receiver without local weather sensors. Missing sensor readings are `null`, displayed as unavailable and exported as blank CSV cells. Firmware weather states 0–3 map to Calibrating, Normal readings, Approaching front and Immediate squall. Normal readings do not certify safe conditions. LoRa RSSI is receiver metadata, not an encoded field of the firmware packet.

The SOS panel is read-only. Its coordinates belong to the sample SOS event; the latest GPS fix can be different. No SOS transmission, rescue dispatch, acknowledgment or cancellation happens from this site.

Desktop places the map next to weather and SOS. Mobile places weather and SOS before the map. Last-known GPS fix and reception age remain visible, and invalid or missing GPS fixes never create a marker.

## Later: Supabase

`src/data.ts` contains the typed `Device`/`Reading`/`SosEvent` models, sample provider, and selectors. Replace the provider with Supabase queries when the firmware fields and public-access policy are defined. Add loading/error states with that asynchronous integration; never silently substitute demo records for failed real queries. Keep secret/service-role keys on the server, use a publishable key in the browser, and apply row-level read policies for public data. No firmware or backend changes are part of this version.

## Geographic map

Leaflet renders OpenStreetMap tiles with visible attribution. Network access is needed for the basemap; telemetry cards and coordinate text remain usable if tiles fail. The Acmac area reference is approximately 8.2740, 124.2649 ([PhilAtlas](https://www.philatlas.com/mindanao/r10/iligan/acmac.html)); it is not a surveyed gateway station. All offshore vessel coordinates are invented. These maps and readings are not navigation charts or official forecasts.

Tiles use the browser cache and fetch only for the visible map. There is no tile prefetch or offline-download feature. Before public deployment, review [OpenStreetMap tile usage policy](https://operations.osmfoundation.org/policies/tiles/) and select a different tile provider if traffic requires it. See the [Leaflet guide](https://leafletjs.com/examples/quick-start/) for map configuration.
