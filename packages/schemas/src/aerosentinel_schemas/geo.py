"""Small WGS84 helpers so modules do not each re-implement them."""
from __future__ import annotations

import math

from .common import GeoPoint

EARTH_RADIUS_M = 6_371_008.8


def haversine_m(a: GeoPoint, b: GeoPoint) -> float:
    p1, p2 = math.radians(a.lat), math.radians(b.lat)
    dphi = p2 - p1
    dlmb = math.radians(b.lon - a.lon)
    h = math.sin(dphi / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlmb / 2) ** 2
    return 2 * EARTH_RADIUS_M * math.asin(math.sqrt(h))


def offset(p: GeoPoint, north_m: float = 0.0, east_m: float = 0.0, alt_m: float | None = None) -> GeoPoint:
    """Point displaced by (north, east) metres (small-distance approximation)."""
    dlat = math.degrees(north_m / EARTH_RADIUS_M)
    dlon = math.degrees(east_m / (EARTH_RADIUS_M * math.cos(math.radians(p.lat))))
    return GeoPoint(lat=p.lat + dlat, lon=p.lon + dlon, alt_m=p.alt_m if alt_m is None else alt_m)


def to_local_m(origin: GeoPoint, p: GeoPoint) -> tuple[float, float]:
    """(east, north) metres of p relative to origin. Matches the Unreal geo contract."""
    north = math.radians(p.lat - origin.lat) * EARTH_RADIUS_M
    east = math.radians(p.lon - origin.lon) * EARTH_RADIUS_M * math.cos(math.radians(origin.lat))
    return east, north
