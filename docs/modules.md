# Module map and status
Status: `skeleton` = interface + baseline exists; `todo` = scaffold only.

| ID | Module | Location | Status | First real work |
|---|---|---|---|---|
| A1 | Schemas & contracts | `packages/schemas` | skeleton, tests | Review & freeze v1.0; JSON Schema export for the dashboard |
| A2 | Data storage | `storage/` | SQL migration + in-memory repo | `PostgresRepository`, MinIO image storage, migration runner |
| A3 | Ingestion | `ingestion/` | validate + load | Streaming (telemetry/images), dead-letter queue |
| A4 | Backend API | `api/` | alerts, risk, health | Regions, observations, history queries, WebSocket fan-out |
| B1 | Geo-simulator | `geosim/` | flood world | Fire spread, terrain, rainfall fields |
| B2 | Synthetic data | `datagen/` | seasonal + flood events | Fire/landslide, Parquet export, labelled windows for D4 |
| B3 | 3D world | `unity/.../Mountain Terrain rocks and tree`, `unity/.../AeroSentinel/Scenes/Training 1.unity` | implemented | Mountain terrain, trees, rocks, camera, lighting, and training-scene integration |
| B4 | Scenario engine | `unity/.../Disasters`, `unity/.../Scenes/Training 1.unity` | implemented | Single-scene time-stepped flood, fire, landslide, and structural-damage scenarios with queryable ground-truth labels and runtime controls |
| B5 | Virtual sensors | `unity/.../Sensors`, `unity/.../Environment/TrainingControlPanel.cs` | implemented | RGB, thermal, water-level, rainfall, soil-moisture, and weather readings with seeded noise, dropout, and on-screen calibration controls |
| C1 | Drone interfaces | `autonomy/mock_drone.py`, `unity_drone.py` | Mock + Unity bridge | Flight-stack adapter (after ADR-0002) |
| C2 | Dock & battery | `autonomy/dock.py` | linear charge | Degradation, readiness rules |
| C3 | Scheduler | `autonomy/scheduler.py` | interval loop | Failure recovery, multi-region, wall-clock mode |
| C4 | Patrol planner | `autonomy/planner.py` | lawn-mower | Obstacles, wind, risk-weighted revisit |
| C5 | Investigation | `autonomy/investigation.py` | approach/orbit/verify | Re-plan on battery, multi-target |
| D1 | Detection | `perception/detection.py`, `train_yolo.py` | threshold + YOLO wrapper | Dataset from Unity, training, eval |
| D2 | Measurement | `perception/measurement.py` | water depth from sensor | Segmentation-based area/depth |
| D3 | Trend & anomaly | `intelligence/trend.py` | z-score, CUSUM, IsolationForest | Per-zone pipelines over history |
| D4 | Risk predictor | `intelligence/risk.py`, `risk_lstm.py` | baseline + LSTM skeleton | Train on B2 data, add XGBoost, calibration |
| D5 | Alert engine | `intelligence/alerts.py` | dedupe + escalate | De-escalation/resolve rules, notification hooks |
| E1 | Dashboard | `dashboard/` | todo | Map, drone marker, risk heatmap, timeline |
| E2 | Integration/QA | `integration/`, CI, compose | e2e smoke test | Unity e2e job, metrics, demo scripts |
