"""B1: lightweight 2D geo-simulator. Fast stand-in for the Unreal world.

Implements the `World` protocol used by MockDrone, so the pipeline runs end-to-end
before any 3D asset exists. Add fire spread, terrain, rain fields here.
"""
from __future__ import annotations

import random
from dataclasses import dataclass, field
from datetime import datetime
from typing import Protocol

from aerosentinel_schemas import BoundingBox, GeoPoint, SensorType
from aerosentinel_schemas.geo import haversine_m


class World(Protocol):
    def sample(self, point: GeoPoint, t: datetime, sensors: list[SensorType]) -> dict[SensorType, dict[str, float]]: ...
    def zone_of(self, point: GeoPoint) -> str: ...


@dataclass
class GridZones:
    region_id: str
    area: BoundingBox
    rows: int = 3
    cols: int = 3

    def zone_of(self, p: GeoPoint) -> str:
        r = min(self.rows - 1, max(0, int((p.lat - self.area.south) / (self.area.north - self.area.south) * self.rows)))
        c = min(self.cols - 1, max(0, int((p.lon - self.area.west) / (self.area.east - self.area.west) * self.cols)))
        return f"{self.region_id}/z-{r}{c}"


@dataclass
class FloodWorld:
    """A circular flood that grows linearly with time. Ground truth is queryable."""
    zones: GridZones
    flood_center: GeoPoint
    start: datetime
    growth_m_per_hour: float = 200.0
    max_depth_m: float = 2.5
    noise_sd: float = 0.02
    rng: random.Random = field(default_factory=lambda: random.Random(0))

    def radius_m(self, t: datetime) -> float:
        return max(0.0, (t - self.start).total_seconds() / 3600 * self.growth_m_per_hour)

    def true_depth(self, p: GeoPoint, t: datetime) -> float:
        r, d = self.radius_m(t), haversine_m(p, self.flood_center)
        return 0.0 if d >= r else self.max_depth_m * (1 - d / r)

    def zone_of(self, p: GeoPoint) -> str:
        return self.zones.zone_of(p)

    def sample(self, point, t, sensors):
        out: dict[SensorType, dict[str, float]] = {}
        for s in sensors:
            if s is SensorType.WATER_LEVEL:
                out[s] = {"water_level_m": max(0.0, self.true_depth(point, t) + self.rng.gauss(0, self.noise_sd))}
            elif s is SensorType.RAINFALL:
                out[s] = {"rain_mm_h": max(0.0, self.rng.gauss(5, 1))}
            elif s is SensorType.SOIL_MOISTURE:
                out[s] = {"soil_moisture": min(1.0, max(0.0, self.rng.gauss(0.5, 0.05)))}
            else:
                out[s] = {}
        return out
