"""E2: end-to-end smoke test on every merge.
dock -> patrol (MockDrone in 2D geo-sim) -> ingest -> detect -> measure -> investigate -> alert -> API.
The same DroneInterface boundary is used by the Unreal adapter for 3D integration."""
from datetime import datetime, timedelta, timezone

from aerosentinel.api.app import create_app
from aerosentinel.autonomy.clock import SimClock
from aerosentinel.autonomy.dock import DockSimulator
from aerosentinel.autonomy.investigation import InvestigationController
from aerosentinel.autonomy.mock_drone import MockDrone
from aerosentinel.autonomy.scheduler import MissionScheduler
from aerosentinel.geosim.world import FloodWorld, GridZones
from aerosentinel.ingestion.pipeline import ingest_observations
from aerosentinel.intelligence.alerts import AlertEngine
from aerosentinel.perception.detection import ThresholdDetector
from aerosentinel.perception.measurement import measure
from aerosentinel.storage.repository import InMemoryRepository
from aerosentinel_schemas import BoundingBox, GeoPoint, HazardType, Severity, new_id
from fastapi.testclient import TestClient

T0 = datetime(2026, 7, 1, tzinfo=timezone.utc)
DOCK = GeoPoint(lat=22.5000, lon=88.3000)
AREA = BoundingBox(south=22.5000, west=88.3000, north=22.5090, east=88.3090)   # ~1 km square


def build():
    clock = SimClock(T0)
    zones = GridZones("region-001", AREA)
    world = FloodWorld(zones, flood_center=GeoPoint(lat=22.5045, lon=88.3045), start=T0 - timedelta(hours=2))
    drone = MockDrone(world, clock, DOCK)
    repo = InMemoryRepository()
    sched = MissionScheduler(drone, clock, DockSimulator(), zones, DOCK, sink=repo.add_observations)
    return clock, zones, drone, repo, sched


def test_full_pipeline_raises_verified_flood_alert():
    clock, zones, drone, repo, sched = build()

    result = sched.run_patrol()
    assert result.observations and drone.battery_level() == 100.0       # recharged after landing
    assert drone.get_state().mode == "docked"

    # round-trip through ingestion validation as the API/ingest path would see it
    assert ingest_observations([o.model_dump(mode="json") for o in result.observations],
                               InMemoryRepository()).rejected == []

    detector, engine = ThresholdDetector(), AlertEngine(repo)
    flagged = [(o, d) for o in result.observations for d in detector.detect(o)]
    assert flagged, "flood should be visible to the patrol"
    obs, det = max(flagged, key=lambda p: p[1].confidence)

    def verifier(evidence):
        hits = [d for o in evidence for d in detector.detect(o)]
        return (len(hits) >= 2, max((d.confidence for d in hits), default=0.0), HazardType.FLOOD)

    verdict = InvestigationController(drone, verifier).investigate(new_id("anom"), obs.position, obs.region_id, obs.zone_id)
    assert verdict.verified

    alert = engine.on_detection(det, obs.region_id, obs.zone_id, verified=verdict.verified, measurement=measure(det, obs))
    again = engine.on_detection(det, obs.region_id, obs.zone_id, verified=True, measurement=measure(det, obs))
    assert again.id == alert.id, "duplicate alerts must be merged"
    assert len(repo.alerts("region-001")) == 1

    client = TestClient(create_app(repo))
    body = client.get("/regions/region-001/alerts").json()
    assert body[0]["hazard"] == "flood" and body[0]["severity"] in {s.value for s in Severity}
