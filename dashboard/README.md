# Dashboard (E1 - member 3)
React + Mapbox GL (or Cesium for 3D): live map, drone position, risk heatmap, alert timeline.
Data: REST + WebSocket from `python/src/aerosentinel/api` (OpenAPI at `/docs`); types from `docs/json_schema/*.json`.
Scaffold: `npm create vite@latest . -- --template react-ts`, then add `react-map-gl` or `cesium`.
Build against the API with the in-memory repo first (`make api`); no drone or Unreal runtime required.
