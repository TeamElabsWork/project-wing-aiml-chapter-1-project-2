"""C1: UnrealDrone - DroneInterface over a TCP JSON-lines simulation bridge.

Protocol (one JSON object per line, UTF-8, '\n' terminated):
  request : {"id": 7, "cmd": "fly_to", "args": {"lat":..,"lon":..,"alt_m":..,"speed_mps":..}}
  response: {"id": 7, "ok": true, "result": {...}}   or   {"id": 7, "ok": false, "error": "..."}
Commands: fly_to, get_state, capture, dock, battery_level, reset.
Swap for a ROS 2 bridge later by re-implementing this one class (see docs/adr/0001-middleware.md).
"""
from __future__ import annotations

import json
import socket
from collections.abc import Sequence
from itertools import count

from aerosentinel_schemas import (
    DroneError,
    DroneInterface,
    DroneState,
    GeoPoint,
    Observation,
    SensorType,
)


class UnrealDrone(DroneInterface):
    def __init__(self, host: str = "127.0.0.1", port: int = 5555, timeout_s: float = 120.0) -> None:
        self._sock = socket.create_connection((host, port), timeout=timeout_s)
        self._file = self._sock.makefile("rw", encoding="utf-8", newline="\n")
        self._ids = count(1)

    def _call(self, cmd: str, **args):
        rid = next(self._ids)
        self._file.write(json.dumps({"id": rid, "cmd": cmd, "args": args}) + "\n")
        self._file.flush()
        reply = json.loads(self._file.readline())
        if not reply.get("ok"):
            raise DroneError(reply.get("error", "unknown Unreal simulation error"))
        return reply["result"]

    def fly_to(self, point: GeoPoint, speed_mps: float = 10.0) -> None:
        self._call("fly_to", lat=point.lat, lon=point.lon, alt_m=point.alt_m, speed_mps=speed_mps)

    def get_state(self) -> DroneState:
        return DroneState.model_validate(self._call("get_state"))

    def capture(self, sensors: Sequence[SensorType], *, region_id: str, zone_id: str, patrol_id: str) -> list[Observation]:
        rows = self._call("capture", sensors=[s.value for s in sensors],
                          region_id=region_id, zone_id=zone_id, patrol_id=patrol_id)
        return [Observation.model_validate(r) for r in rows]

    def dock(self) -> None:
        self._call("dock")

    def battery_level(self) -> float:
        return float(self._call("battery_level")["battery_pct"])

    def close(self) -> None:
        self._file.close()
        self._sock.close()
