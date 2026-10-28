# AeroSentinel

### Autonomous Drone-Based Disaster Monitoring & Early Warning System

AeroSentinel is an autonomous, simulation-first disaster-monitoring system. A drone permanently stationed at a docking station patrols a disaster-prone region, learns how the environment is changing, and reports risks before they become severe.

## The Problem

Disaster monitoring is often reactive:

- Floods are identified after water has already spread.
- Forest fires are identified after smoke or flames become visible.
- Infrastructure damage is identified after it becomes significant.

Environmental conditions are dynamic, but monitoring is frequently periodic, manual, or dependent on people reaching the affected location. AeroSentinel addresses this gap by building a continuous historical record of the environment instead of relying only on isolated snapshots.

## The AeroSentinel Flow

At fixed intervals, the drone automatically:

```text
Deploys -> Patrols -> Senses -> Analyzes -> Reports
    ^                                           |
    |                                           v
Recharges <- Returns <------------------- Investigation
```

During each patrol, it collects:

- RGB imagery
- Thermal data
- Environmental sensor data
- Location and movement data
- Infrastructure condition data

The observations are ingested, stored, analyzed, and exposed through the API and dashboard:

```text
Drone / Unity simulation
        |
        v
Observation -> Ingestion -> Historical storage
                              |
                 +------------+------------+
                 |                         |
                 v                         v
          Disaster detection       Trend and risk prediction
                 |                         |
                 +------------+------------+
                              v
                     Alert and investigation
                              |
                              v
                       API -> Dashboard
```

## Where the Intelligence Comes In

### 1. Disaster Detection

When the drone discovers an event that has already happened, AeroSentinel can detect and investigate:

- Floods
- Fires
- Landslides
- Structural damage
- Other environmental anomalies

The system can move the drone closer, collect additional imagery and sensor readings, verify the anomaly, estimate its severity, record its GPS location, and generate an alert.

### 2. Disaster Risk Prediction

AeroSentinel also looks for changes that may indicate an emerging disaster.

For example, a sequence of water-level observations:

```text
Day 1 -> 2.1 m
Day 2 -> 2.3 m
Day 3 -> 2.6 m
Day 4 -> 2.9 m
Day 5 -> 3.3 m
```

is not treated as unrelated readings. The system identifies the trend and combines it with rainfall, soil moisture, terrain, and visual information to produce an early warning such as:

> Flood risk is increasing in this region.

The same architecture can support forest-fire risk, landslide risk, infrastructure deterioration, and other environmental threats.

## The Unique Part

AeroSentinel does not only ask:

> What is happening?

It also asks:

> How is the environment changing?

And eventually:

> Where should the drone investigate next?

Routine patrols can switch to investigation mode when an anomaly is detected. This closes the loop between sensing, verification, planning, and reporting.

## Simulation-First Development

The system is being developed simulation-first so dangerous scenarios can be tested without putting people or real aircraft at risk. The simulation includes:

- Disaster zones
- Roads and buildings
- Rivers and terrain
- Time-evolving flood, fire, landslide, and structural-damage scenarios with ground-truth labels
- Autonomous drones
- Virtual RGB, thermal, water-level, rainfall, soil-moisture, and weather sensors with configurable noise
- Docking stations

The current software stack supports a 2D geospatial simulation and Unity integration. Simulated observations can also generate long historical sequences for training and evaluating prediction models.

## Technology

The planned and implemented components include:

- **Simulation:** Unity 3D, with an extensible path toward AirSim / Unreal Engine
- **Flight and autonomy:** Python autonomy modules, with a path toward ROS 2 + PX4
- **Perception:** Python, PyTorch, YOLO, and OpenCV
- **Prediction:** Scikit-learn, XGBoost, pandas, and LSTM-based models
- **Backend:** FastAPI, PostgreSQL/PostGIS, and TimescaleDB
- **Dashboard:** React with Mapbox or Cesium
- **Contracts:** Versioned Pydantic schemas shared across the Python services

## Architecture

```text
                +-----------------------------------------------+
  Unity 3D  --> | DroneInterface (MockDrone | UnityDrone | ...) | <-- RealDrone
                +----------------------+------------------------+
                                       |
                         Scheduler -> Planner -> Investigation
                                       |
                 Ingestion -> Storage -> Detection -> Measurement
                                       |             |
                                       |       Trend -> Risk
                                       |             |
                                       +---------> Alerts -> API -> Dashboard
```

## Quick Start

```bash
python -m venv .venv

# Windows
.venv\Scripts\activate

# Linux/macOS
source .venv/bin/activate

make install            # schemas, Python package, and development tools
make test               # unit tests and end-to-end smoke test
make demo               # simulated patrol in the 2D geo-sim
make api                # FastAPI on http://localhost:8000
make install-ml         # optional heavy ML dependencies
```

API documentation is available at `http://localhost:8000/docs` after starting the API. For Unity setup, see [`unity/AeroSentinel/README.md`](unity/AeroSentinel/README.md). To start the database, object storage, and API services together:

```bash
docker compose up
```

## Repository Layout

| Path | Responsibility |
|---|---|
| [`packages/schemas/`](packages/schemas/) | Versioned contracts and `DroneInterface` |
| [`python/src/aerosentinel/storage/`](python/src/aerosentinel/storage/) | Storage repository and PostGIS/Timescale migrations |
| [`python/src/aerosentinel/ingestion/`](python/src/aerosentinel/ingestion/) | Observation ingestion |
| [`python/src/aerosentinel/api/`](python/src/aerosentinel/api/) | FastAPI service |
| [`python/src/aerosentinel/geosim/`](python/src/aerosentinel/geosim/) | Geospatial simulation |
| [`python/src/aerosentinel/autonomy/`](python/src/aerosentinel/autonomy/) | Scheduling, planning, docking, and investigation |
| [`python/src/aerosentinel/perception/`](python/src/aerosentinel/perception/) | Detection and measurement |
| [`python/src/aerosentinel/intelligence/`](python/src/aerosentinel/intelligence/) | Trends, risk prediction, and alerts |
| [`unity/AeroSentinel/`](unity/AeroSentinel/) | Unity simulation components |
| [`dashboard/`](dashboard/) | Dashboard scaffold |
| [`integration/`](integration/) | End-to-end tests |

See [`docs/architecture.md`](docs/architecture.md), [`docs/contracts.md`](docs/contracts.md), and [`docs/modules.md`](docs/modules.md) for more detail. Ownership and review responsibilities are documented in [`OWNERS.md`](OWNERS.md).

## Working Principles

1. **Contracts first.** Shared schemas are versioned before dependent implementations change.
2. **One boundary to the drone.** Only `DroneInterface` communicates with a drone; switching from simulation to a real vehicle is an integration/configuration change.
3. **Build against mocks.** Modules can run with `MockDrone`, `FloodWorld`, and `InMemoryRepository` without Unity or a database.
4. **Investigate before alerting.** Detection can trigger an evidence-gathering mission rather than relying on a single observation.
5. **Track change over time.** Historical observations are first-class data for trend analysis and risk prediction.
6. **Use consistent geography and time.** The system uses WGS84, UTC-aware timestamps, and `region-xxx` / `region-xxx/z-yy` identifiers.

## Vision

AeroSentinel can evolve from one drone into a network of autonomous disaster-monitoring stations. Each station continuously observes its region, AI detects abnormal changes, historical data estimates emerging risks, and the system investigates and reports important events automatically.

**From disaster response to continuous disaster intelligence.**
