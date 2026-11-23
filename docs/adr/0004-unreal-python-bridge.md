# ADR-0004: Unreal <-> Python bridge (PROPOSED)
The Unreal project is the active 3D simulator and is stored under `unreal/AeroSentineal/`.
The Python `UnrealDrone` adapter preserves the `DroneInterface` boundary and uses a transport-neutral
JSON-lines command contract. The Unreal-side command server is a follow-up integration task; the
current Unreal project already exports image, telemetry, sensor, and ground-truth records to disk.
