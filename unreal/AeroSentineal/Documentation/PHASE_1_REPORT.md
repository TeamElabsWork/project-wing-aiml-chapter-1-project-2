# Phase 1 — Project Foundation — Report

Date: 2026-11-22 · Engine: **UE 5.8.4** · Project: `AeroSentineal`

## What was created

### C++ core (9 files, module `AeroSentineal`)

| File | Contents |
|---|---|
| `Source/AeroSentineal/Core/AeroLog.h` / `.cpp` | `LogAeroSentinel` log category. |
| `Source/AeroSentineal/Core/AeroTypes.h` | `EAeroDroneState`, `EAeroDisasterType`, `EAeroDisasterPhase`, `EAeroZoneId`, `EAeroDamageState`, `FAeroGeoPose`. |
| `Source/AeroSentineal/Core/AeroSimSettings.h` / `.cpp` | `UAeroSimSettings` (`UDeveloperSettings`) — dataset root, drone id, default time multiplier, UTC start, WGS84 origin, `GetAbsoluteDatasetRoot()`, `GetStartUtc()`. |
| `Source/AeroSentineal/Core/AeroSimClockSubsystem.h` / `.cpp` | `UAeroSimClockSubsystem` (`UTickableWorldSubsystem`) — the only simulation clock. `SimulationSeconds`, `TimeMultiplier`, pause, `SimulationUtc`, `GetSimulationUtcIso8601()`, `AdvanceSimulationTime()` for batch runs, `OnTimeAdvanced` delegate. |
| `Source/AeroSentineal/Core/AeroGeoSubsystem.h` / `.cpp` | `UAeroGeoSubsystem` (`UWorldSubsystem`) — `EngineToGeographic`, `GeographicToEngine`, `GetGeoPose`, `FormatGeoCoordinate`, backed by `AGeoReferencingSystem`. |

`Source/AeroSentineal/AeroSentineal.Build.cs` now declares `PublicIncludePaths.Add(ModuleDirectory)` and the
dependencies `GeoReferencing`, `EnhancedInput`, `GameplayTags` (public), `DeveloperSettings`, `Json`,
`JsonUtilities`, `ImageWrapper`, `RenderCore`, `UMG`, `Slate`, `SlateCore` (private).

### Content + disk structure

* 16 content folders under `/Game/AeroSentinel/` (`Core`, `World/{Levels,Landscape,Materials,PCG,Splines}`,
  `Drone/{Meshes,Materials}`, `Sensors`, `Disasters/{Niagara,Materials}`, `UI`, `Data`).
* `Simulation/{Datasets/{RGB,Thermal,Annotations,GroundTruth,Telemetry,Sensors,Events},Exports,Config,Tools}`.
* `Blender/{sources,exports}` — asset pipeline staging.
* `Documentation/{AEROSENTINEL_ARCHITECTURE,THIRD_PARTY_ASSETS,PHASE_1_REPORT}.md`.
* `Simulation/Config/aerosentinel_sim.json` — the config contract the external Python modules read.

### Environment

* `/Game/AeroSentinel/World/Levels/L_AeroValley` — **World Partition** level, registered as both
  `EditorStartupMap` and `GameDefaultMap` (the previous `GameDefaultMap=/Game/Main` pointed at a
  non-existent asset).
* `Terrain_AeroValley001` — landscape 1009×1009, 16×16 components @ 200 cm/vertex = **2016 × 2016 m**,
  surface Z 20.69 m → 123.11 m.
* Lighting: `Sun_Directional` (movable), `SkyAtmosphere`, `SkyLight` (movable, real-time capture),
  `HeightFog`, `PlayerStart_Village`, `WorldDataLayers`, `WorldPartitionMiniMap`.
* Terrain collision enabled on all 256 landscape components (`QUERY_AND_PHYSICS` / `BlockAll`).

### Coordinate contract (bound and verified)

`GeoReferencingSystem` actor in the level: EPSG:4326 geographic, EPSG:32631 projected, Flat Planet,
origin-in-projected-CRS **off**, origin **46.4813 N / 6.9104 E / 380.0 m**. World origin (0,0,0) is the
region's south-west corner. Mirrored into `UAeroSimSettings`.

### Drone asset

Authored procedurally in Blender (`Blender/sources/SM_AeroDrone.blend`) and imported via glTF:

| Asset | Dimensions | Tris | Material slots |
|---|---|---|---|
| `SM_AeroDrone_Body` | 41.8 × 41.8 × 21.1 cm | 810 | 3 |
| `SM_AeroDrone_Propeller` | 25.0 × 2.5 × 1.3 cm | 84 | 1 |
| `SM_AeroDrone_Gimbal` | 8.1 × 7.4 × 7.0 cm | 386 | 4 |

Materials `MAT_Drone_Carbon / Shell / Accent / Glass` at `/Game/AeroSentinel/Drone/Materials/`.
Propeller and gimbal are **separate meshes** so Phase 3 can spin the four rotors and aim the gimbal
independently. Register entry: `Documentation/THIRD_PARTY_ASSETS.md` row 1.

## How to run it

1. Open the project — it loads `L_AeroValley` (startup map).
2. Project Settings → **Game → AeroSentinel Simulation** to change dataset root, time multiplier, UTC
   start or the WGS84 origin.
3. Press Play. The simulation clock ticks `UAeroSimClockSubsystem` (`LogAeroSentinel` in the output log
   prints clock resets and multiplier changes).

## Test results

| Test | Result |
|---|---|
| Module compiles (`Live Coding`) | **Succeeded** — no warnings or errors |
| `load_class` for `AeroSimClockSubsystem`, `AeroGeoSubsystem`, `AeroSimSettings` | **All 3 loaded** |
| Settings CDO read-back | `mult=1.0`, `start=2026-06-01T06:00:00.000Z`, `drone=DRN-001` |
| Content folders created | 16 / 16 |
| Disk directories created | 12 |
| Config JSON written + re-parsed | OK |
| Landscape collision trace grid (via level build) | 441 / 441 hits |
| Geo round trip world (0,0,0) → WGS84 → world | 46.481300, 6.910400, 380.000000 → **(0, 0, 0)** |
| Geo 1 km east | +0.012997° longitude, −0.000446° latitude (UTM 31N convergence — correct) |
| Drone mesh scale after import | 41.8 cm motor span, 13 cm propeller radius — real-world correct |
| Editor restart from a rebuilt module | Editor reopened on `L_AeroValley` (startup map change works); all 3 classes loaded with fresh bindings |
| **PIE: clock ticks** | `sim_seconds = 1695.0` after ~28 real seconds at x60; `utc = 2026-06-01T06:28:15.000Z` (= start + 1695 s) |
| **PIE: geo subsystem resolves at runtime** | `is_geo_reference_ready = true`; landscape centre (100000, 100000, 7000) cm → 46.471876 N, 6.922751 E, **450.00 m** (= 380 m origin + 70 m terrain) |
| **PIE: false georeference warning removed** | Second run logged only the clock reset line; no "No GeoReferencingSystem" warning |
| Settings ini persistence | **Confirmed after a clean rebuild + restart**: origin 46.4813 / 6.9104 / 380.0, drone `DRN-001`, start 2026 all loaded from `[/Script/AeroSentineal.AeroSimSettings]` in `Config/DefaultGame.ini` |
| Dataset root resolution | `GetAbsoluteDatasetRoot()` → `D:/UnrealEngine/AeroSentineal/Simulation/Datasets` (fixed — `FPaths::ProjectDir()` is relative inside an editor process, so the path is now routed through `IFileManager`) |

## What remains incomplete

1. **Drone is not yet an actor.** The three meshes exist and are correct, but there is no
   `AAeroDronePawn`, no propeller rotation, no gimbal control, no battery — all Phase 3.
2. **Terrain is bare.** No vegetation, roads, river water body, buildings or PCG — Phase 2.
3. **Landscape material is a stopgap** (engine tutorial landscape material with `Dirt / Grass / Snow`
   paint layers bound). It renders correctly but must be replaced by an AeroSentinel landscape material
   in the Phase 2 art pass; the paint layers are already wired.
4. **No zone volumes yet.** `EAeroZoneId` exists as a type but nothing in the level is tagged with a
   zone, so nothing can currently answer "which region am I in?" — Phase 2.
5. **No water body.** The river is a terrain channel only; the flood system is Phase 4 (and needs the
   Experimental `Water` plugin enabled, which requires an editor restart).
6. **No HUD, no sensors, no disasters, no data writers** — Phases 4–8.
7. Region size is **2.0 × 2.0 km**, not the ~8 × 8 km a full production region would want. That is a
   deliberate Phase 1 choice (fast iteration, small WP cell count); the landscape can be extended or
   replaced in Phase 2 once the zone layout is final.

## Dependencies for Phase 2

* `AAeroZoneVolume` + `AAeroRegionDirector` (new C++) — must exist before vegetation/building scatter,
  because PCG graph parameters and ground-truth `region_id` both key off zone identity.
* A real landscape material (AeroSentinel art pass) — replaces the tutorial stopgap.
* PCG framework verification: confirm the `PCG` plugin is enabled in this project before authoring graphs.
* Spline-based road and river authoring (roads Phase 2, water Phase 4).
