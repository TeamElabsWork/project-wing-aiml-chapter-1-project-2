# AeroSentinel — System Architecture

Autonomous drone-based disaster monitoring and early-warning simulation.

| Item | Value |
|---|---|
| Engine | Unreal Engine 5.8.4 (`D:\UE_5.8`) |
| Project file | `AeroSentineal.uproject` |
| C++ module | `AeroSentineal` (Runtime, single game module) |
| Parent template | Unreal "Simulation Blank" (SimBlank) |
| Enabled plugins | ModelingToolsEditorMode, SunPosition, **GeoReferencing**, Aura |
| Renderer | DX12 / SM6, Lumen GI+Reflections, Nanite, Substrate enabled, Ray Tracing on |

> **Naming note:** the on-disk project, module and API macro are spelled `AeroSentineal`
> (as created). All *content* assets and logical systems use the spelling **AeroSentinel**
> (`/Game/AeroSentinel/...`). Do not rename the module — it would invalidate the existing
> `.uproject`, redirects in `DefaultEngine.ini` and `BP_SimGameMode`.

---

## 1. Module topology (logical vs physical)

The spec's B1–B5 / C1 modules are **logical** boundaries. They are physically expressed as
folder-level packages inside one C++ module, plus three process-external modules.

Creating additional Unreal *code modules* (`.Build.cs` + plugin descriptors) is deliberately
deferred: a single game module compiles faster, has no cross-module API churn, and the
`AeroSentinel` logical folders are already include-path addressable (`Core/AeroTypes.h`).
The split into `AeroSentinelCore / AeroSentinelSensors / AeroSentinelDisasters` plugin modules
is a Phase-8 optimisation only if compile times demand it.

| ID | Module | Runs in | Responsibility | Phase |
|---|---|---|---|---|
| **B1** | Lightweight Geographical Simulator | External Python (repo `Simulation/Tools`) | Author terrain heightfield, zone polygons, road/river splines, region metadata → `Simulation/Config/*.json`. Never runs in UE. | 1–2 |
| **B2** | Synthetic Long-Horizon Data Generator | External Python | Drives the sim clock / mission loop headlessly, accumulates months of records, post-processes `Datasets/`. | 7 |
| **B3** | UE 3D World Builder | Unreal (Content + `AeroSentineal/World`) | Level, Landscape, World Partition, PCG, splines, lighting/weather. | 1–2 |
| **B4** | Disaster Scenario Engine | Unreal (`AeroSentineal/Disasters`) | `UAeroDisasterSubsystem` + per-type disaster actors, severity/progression, ground truth. | 4 |
| **B5** | Virtual Sensor Suite | Unreal (`AeroSentineal/Sensors`) | Sensor components, render targets, noise models, timestamped samples. | 5 |
| **C1** | Drone Interface | Unreal (`AeroSentineal/Drone`) | Airframe, FSM, waypoint flight, battery, docking, return-to-home, FC adapter seam. | 3 |
| **—** | Core | Unreal (`AeroSentineal/Core`) | Types, settings, **sim clock**, **geo transform**, event bus, logging. | 1 |
| **—** | Data | Unreal (`AeroSentineal/Data`) | Async JSON/image writers, dataset layout, record IDs. | 7 |

### Dependency rule (enforced by include direction)

```
Core  <-  World  <-  Disasters
  ^         ^            ^
  |         |            |
  +------ Drone -------- +   (drone depends on Core + Sensors only)
            ^
            |
        Patrol  ->  Data  ->  (external B1/B2 via files)
```

* `Disasters` **never** includes `Drone`. Disasters publish ground truth + gameplay tags;
  the drone and the anomaly detector observe them, never the reverse.
* `Patrol` never includes a concrete disaster class — it reacts to `FAeroAnomalyRequest`.
* `Data` never includes `Disasters`; it serialises generic records passed to it.
* `Core` includes nothing from the folders above it.

---

## 2. Coordinate + time contract (project-wide, frozen in Phase 1)

**Geo.** WGS84 throughout, handled by `AGeoReferencingSystem` (GeoReferencing plugin),
wrapped by `UAeroGeoSubsystem`. One authoritative pair:

* Geographic CRS `EPSG:4326` (WGS84 lat/lon degrees, altitude metres).
* Projected CRS `EPSG:32631` (UTM 31N) — chosen as the default origin frame; the world is a
  **Flat Planet** projection because the playable region is ~8 × 8 km.
* Unreal world origin (0,0,0) ↔ one fixed WGS84 point, set identically in
  `Project Settings → Game → AeroSentinel Simulation` and on the level's
  `GeoReferencingSystem` actor. Every record's lat/lon comes from
  `UAeroGeoSubsystem::EngineToGeographic` — no hand-rolled conversion anywhere else.

**Time.** `UAeroSimClockSubsystem` owns the only simulation clock.
`SimulationUtc = StartUtc + SimulationSeconds`. All records store `timestamp_utc` as
ISO-8601 (`.ToIso8601()`, millisecond precision, Z suffix). Accelerated time is
`TimeMultiplier` sim-seconds per real second, applied in `Tick`; batch generation calls
`AdvanceSimulationTime()` for deterministic stepping with no frames.

**IDs.** `drone_id` from settings (`DRN-001`), `disaster_id` = `DSR-<type>-<zero-padded seq>`,
`region_id` = `EAeroZoneId` name. Record ids are `"<kind>-<timestamp>-<counter>"`.

---

## 3. Folder structure

```
AeroSentineal/
├─ Source/AeroSentineal/
│  ├─ AeroSentineal.Build.cs
│  ├─ Core/        AeroLog.h/.cpp, AeroTypes.h, AeroSimSettings.h/.cpp,
│  │               AeroSimClockSubsystem.h/.cpp, AeroGeoSubsystem.h/.cpp      [PHASE 1 DONE]
│  ├─ World/       AeroZoneVolume, AeroRegionDirector                        [PHASE 2]
│  ├─ Drone/       AeroDronePawn, AeroDroneController, AeroBatteryComponent,
│  │               AeroWaypointFollower, AeroDockingStation                  [PHASE 3]
│  ├─ Disasters/   AeroDisasterSubsystem, AeroDisasterBase, AeroFloodActor,
│  │               AeroFireActor, AeroLandslideActor, AeroStructureActor,
│  │               AeroGroundTruthRecorder                                   [PHASE 4]
│  ├─ Sensors/     AeroSensorComponent, AeroRgbCameraComponent,
│  │               AeroThermalCameraComponent, AeroWaterLevelSensor,
│  │               AeroRainfallSensor, AeroSoilMoistureSensor,
│  │               AeroWeatherSensor, AeroNoiseModel                         [PHASE 5]
│  ├─ Patrol/      AeroPatrolSubsystem, AeroInvestigationComponent,
│  │               AeroRegionDataAsset, AeroAnomalyRequest                   [PHASE 6]
│  └─ Data/        AeroDataSubsystem, AeroJsonWriter, AeroImageWriter        [PHASE 7]
│
├─ Content/AeroSentinel/
│  ├─ Core/        BP_AeroGameMode, BP_AeroPlayerController, BP_AeroGameInstance
│  ├─ World/       Levels/L_AeroValley, Landscape/, Materials/, PCG/, Splines/
│  ├─ Drone/       Meshes/, BP_Drone, BP_DockingStation
│  ├─ Sensors/     M_Thermal, RT_*, BP_SensorRig
│  ├─ Disasters/   Niagara/, Materials/, BP_Disaster*
│  ├─ UI/          WBP_AeroHUD, WBP_DevPanel
│  └─ Data/        DA_Zones, DA_PatrolRoutes, Curves/
│
├─ Simulation/                  <- external-facing, never cooked
│  ├─ Datasets/{RGB,Thermal,Annotations,GroundTruth,Telemetry,Sensors,Events}/
│  ├─ Exports/
│  ├─ Config/                   aerosentinel_sim.json (B1/B2 contract)
│  └─ Tools/                    (B1/B2 Python, Phase 2/7)
│
├─ Blender/                     .blend sources + export scripts (asset pipeline)
│  └─ exports/                  staged FBX/glTF handed to UE
│
└─ Documentation/
   ├─ AEROSENTINEL_ARCHITECTURE.md   (this file)
   ├─ THIRD_PARTY_ASSETS.md          (licence register — mandatory)
   └─ PHASE_1_REPORT.md              (per-phase report)
```

---

## 4. Class architecture

### Phase 1 — implemented

| Class | Type | Role |
|---|---|---|
| `UAeroSimSettings` | `UDeveloperSettings` | Config surface: dataset root, drone id, default multiplier, UTC start, WGS84 origin. One source of truth for B1/B2 handoff. |
| `UAeroSimClockSubsystem` | `UTickableWorldSubsystem` | The simulation clock. Owns `SimulationSeconds`, `TimeMultiplier`, pause, `SimulationUtc`, `OnTimeAdvanced`. Manual `AdvanceSimulationTime()` for batch runs. |
| `UAeroGeoSubsystem` | `UWorldSubsystem` | Wraps `AGeoReferencingSystem`. `EngineToGeographic`, `GeographicToEngine`, `GetGeoPose`, `FormatGeoCoordinate`. |
| `EAeroDroneState`, `EAeroDisasterType`, `EAeroDisasterPhase`, `EAeroZoneId`, `EAeroDamageState`, `FAeroGeoPose` | enums / struct | Shared vocabulary used by every later module. |

### Phase 2–8 — implementation status

| Phase | Classes | Notes |
|---|---|---|
| 2 | `AAeroZoneVolume`, `UAeroRegionSubsystem`, `AAeroWeatherDirector` | Implemented in C++; level/content coverage still needs completion and validation for all five zones. |
| 3 | `AAeroDronePawn`, `UAeroBatteryComponent`, `AAeroDockingStation` | Implemented controlled-kinematic FSM, battery, docking, sensors, and external-control seam. |
| 4 | `UAeroDisasterSubsystem`, `AAeroFloodActor`, `AAeroFireActor`, `AAeroLandslideActor`, `AAeroStructureActor` | Implemented simplified evolving hazards and JSONL ground truth; Niagara/Chaos and physically based water remain future work. |
| 5 | `UAeroSensorComponent` + RGB, thermal, weather, gas, water-level, rainfall, soil-moisture, telemetry components | Implemented timestamped noisy records. Thermal is currently false-colour, not radiometric. |
| 6 | `UAeroPatrolSubsystem` | Implemented decoupled queued anomaly requests and investigation hand-off; external transport and richer route planning remain future work. |
| 7 | `AeroRecordWriter` plus sensor-owned writers | Implemented synchronous JSON/PNG records and annotations; centralized async data subsystem and event stream remain future work. |
| 8 | `WBP_AeroHUD`, `WBP_DevPanel` | Read-only telemetry + test controls; controls call the same public API the autonomy layer uses. |

---

## 5. Data contract

Every record written to `Simulation/Datasets/**` carries the mandatory header:

```json
{
  "record_id": "SENSOR-2026-06-01T06:14:03.250Z-000182",
  "timestamp_utc": "2026-06-01T06:14:03.250Z",
  "latitude": 46.4812731,
  "longitude": 6.9104382,
  "altitude_m": 412.75,
  "region_id": "RiverFloodplain",
  "drone_id": "DRN-001",
  "disaster_id": "DSR-FLOOD-0003"
}
```

| Folder | Content | Producer |
|---|---|---|
| `RGB/` | drone RGB frames + `*.meta.json` | `UAeroRgbCameraComponent` |
| `Thermal/` | thermal frames + per-pixel temperature range | `UAeroThermalCameraComponent` |
| `Annotations/` | boxes/masks per frame from ground truth | `UAeroDataSubsystem` |
| `GroundTruth/` | disaster state per tick (depth, burnt area, debris, damage state) | `UAeroDisasterSubsystem` |
| `Telemetry/` | ECEF pose, speed, battery, FSM state | `UAeroDronePawn` |
| `Sensors/` | measured vs ground-truth pairs | `UAeroSensorComponent` |
| `Events/` | anomaly requests, investigations, mission begin/end | `UAeroPatrolSubsystem` |

---

## 6. Implementation roadmap

| Phase | Exit criteria |
|---|---|
| **1 Foundation** | ✅ Project structure, C++ core (clock/geo/settings/types) compiling, content tree, asset pipeline convention, base terrain level, first drone mesh. |
| 2 Environment | 5 zones placed and tagged on the partitioned landscape, PCG vegetation/rocks, spline roads + river, weather director switching presets, ≥60 fps at 1440p with empty drone. |
| 3 Drone | FSM reaches all 9 states; takeoff→waypoint→land→charge cycle completes unattended; battery depletion forces RTH. |
| 4 Disasters | 4 disaster types each expose parameters, evolve over sim time, and emit ground truth; severity curve matches the configured ramp. |
| 5 Sensors | 6 sensors emit timestamped `{ground_truth, measured}` at configured Hz with noise; thermal output is genuinely temperature-mapped, not tinted RGB. |
| 6 Autonomy | Patrol → anomaly request → investigation → resume/return runs unattended for ≥1 simulated day with no manual input. |
| 7 Data | ≥1 month of synthetic observation records generated by batch stepping; every dataset folder populated; external Python can consume it. |
| 8 Integration | Full end-to-end demo of spec §16, filmed, with dataset export verified against the record contract. |

---

## 7. Known constraints & deliberate decisions

1. **Water plugin is Experimental in 5.8** and not yet enabled. Flood will enable `Water` +
   `WaterExtras` in Phase 4 (requires an editor restart). Until then the flood model is a
   controlled water-surface/volume system, documented as such — no claim of fluid simulation.
2. **World Partition** is required for the large region, so Phase 2 creates the map as a
   partitioned world; the SimBlank map is a non-partitioned template and is kept only as
   the dev sandbox.
3. **Substrate is ON** in this project. Thermal/water materials must be authored Substrate-first;
   the Phase 5 temperature-to-colour mapping will be a Substrate graph, not the legacy
   `ThinTranslucent` path.
4. **Scene Capture is view-dependent**; thermal/RGB capture uses dedicated `SceneCaptureComponent2D`
   with explicit `ShowOnly`/rendering flags so the drone payload does not capture editor helpers.
5. **No ML inside Unreal.** Anomaly detection is an external process; the sim only emits
   requests (`FAeroAnomalyRequest`) through the file/HTTP seam, and a sim-only debug trigger
   exists for testing.
6. **Sketchfab assets** are only pulled with a verified licence; the register lives in
   `Documentation/THIRD_PARTY_ASSETS.md` and is updated in the same commit as the import.
