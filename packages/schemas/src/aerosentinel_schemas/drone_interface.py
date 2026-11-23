"""DroneInterface: the single hardware-abstraction boundary (section 1.1).

Implementations: MockDrone, UnrealDrone (transport adapter), a flight-stack adapter, RealDrone (Plan 2).
No other module may depend on which one is active.
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Sequence

from .common import GeoPoint, SensorType
from .models import DroneState, Observation


class DroneInterface(ABC):
    @abstractmethod
    def fly_to(self, point: GeoPoint, speed_mps: float = 10.0) -> None:
        """Blocking move to `point`. Raise DroneError on failsafe / failure."""

    @abstractmethod
    def get_state(self) -> DroneState: ...

    @abstractmethod
    def capture(
        self,
        sensors: Sequence[SensorType],
        *,
        region_id: str,
        zone_id: str,
        patrol_id: str,
    ) -> list[Observation]:
        """Sample the given sensors at the current position."""

    @abstractmethod
    def dock(self) -> None:
        """Return to the dock and land. Blocking."""

    @abstractmethod
    def battery_level(self) -> float:
        """Battery percentage 0..100."""


class DroneError(RuntimeError):
    """Raised by implementations on failsafe triggers or command failures."""
