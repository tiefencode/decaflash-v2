# Decaflash Cloud Worker

Optionaler, aus V1 übernommener Cloudflare-Worker für einen möglichen Mainframe-API-Vertrag. Projektstand und Prioritäten stehen in der [zentralen README](../../README.md). Eine eigene V2-Deployment-Verknüpfung ist nicht bestätigt.

Current routes:

- `POST /api/chattie`
- `POST /api/audd`
- `GET /api/health`
- `GET /api/debug/last.wav`
- `GET /api/debug/last.json`

## Local Development

```bash
# From the repository root
cd workers/decaflash
cp .dev.vars.example .dev.vars
npm install
npm run dev
```

Required local secrets in `.dev.vars`:

- `MAINFRAME_SHARED_SECRET`
- `AUDD_API_TOKEN`
- `OPENAI_API_KEY`
- optional: `OPENAI_MODEL`

## Deployment-Status

Die mitkopierte Worker-Konfiguration stammt aus V1. Worker-Name, Ressourcenbindungen und Beispielkonfiguration sind keine eingerichtete V2-Umgebung. Den alten Worker oder seine KV-Daten nicht durch ein V2-Deployment überschreiben.

Cloud-Deployment ist aktuell nicht beauftragt. Bei einer späteren Einrichtung braucht V2 eine eigene, geprüfte Zuordnung von Worker, KV, Secrets und Mainframe-Endpunkt. Der vorgesehene Weg ist Git/Cloudflare-Integration; lokales Wrangler-Deployment wird nicht vorausgesetzt.

## Debug Artifacts

`/api/audd` stores the most recent decoded WAV plus metadata for debugging.

Required Cloudflare setup:

- create a KV namespace, e.g. `decaflash-debug-artifacts`
- bind it as `DEBUG_ARTIFACTS`
- keep the binding in `wrangler.jsonc` via `kv_namespaces`
- treat `wrangler.jsonc` as the source of truth for the binding when deploying from Wrangler or Git

Current implementation notes:

- debug artifacts are read and written only through Workers KV
- there is no `caches.default` fallback for `/api/debug/last.wav` or `/api/debug/last.json`
- `/api/debug/*` returns `404` when the key does not exist and `500` when the KV binding is missing

The worker overwrites the same two keys on every decoded `/api/audd` run:

- `debug:last:wav`
- `debug:last:json`

That means the namespace does not grow without bounds for this debug flow.

For a separately configured V2 environment, set `V2_WORKER_URL` to its verified base URL. After a fresh `/api/audd` run, download the latest debug audio:

```bash
curl -fL \
  -H "Authorization: Bearer $MAINFRAME_SHARED_SECRET" \
  "${V2_WORKER_URL:?Set the verified V2 Worker URL}/api/debug/last.wav" \
  -o "$HOME/Downloads/decaflash-last.wav"
```

Download the matching metadata:

```bash
curl -fL \
  -H "Authorization: Bearer $MAINFRAME_SHARED_SECRET" \
  "${V2_WORKER_URL:?Set the verified V2 Worker URL}/api/debug/last.json" \
  -o "$HOME/Downloads/decaflash-last.json"
```

Quick sanity check on macOS:

```bash
file "$HOME/Downloads/decaflash-last.wav"
afplay "$HOME/Downloads/decaflash-last.wav"
```

## Mainframe Contract

`/api/chattie` expects JSON:

```json
{
  "instructions": "Prompt instructions",
  "input": "User input"
}
```

Response:

```json
{
  "text": "Kurzer Text"
}
```

`/api/audd` expects multipart form data with:

- `encoding=mulaw`
- `sample_rate_hz`
- `sample_count`
- `container=decaflash_mulaw`
- `file=<binary mulaw blob>`

Response:

```json
{
  "matched": true,
  "title": "Song Title",
  "artist": "Artist Name",
  "text": "Kurzer Matrix-Text"
}
```
