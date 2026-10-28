# Unity project (B3, B4, B5, Unity side of C1)

Unity owns the 3D world, disaster events and virtual sensors. Python owns everything else and drives Unity
through `UnityDrone` (TCP bridge, JSON lines, default port 5555) - the same `DroneInterface` as `MockDrone`.

`Disasters/DisasterScenarioEngine.cs` advances scenarios in simulation time and emits ground-truth labels for
flood, fire, landslide, and structural-damage events. `Sensors/SyntheticSensor.cs` exposes a six-channel
`ReadSuite()` with deterministic Gaussian noise and configurable dropout. Each `SensorReading` retains the
un-noised `GroundTruth` value so model evaluation can compare predictions with the correct answer.

The single training scene also includes `Environment/TrainingControlPanel.cs`. Press Play to control disaster
type/severity/radius, simulation speed, rainfall, temperature, wind, fog, terrain height, and each sensor's
noise/dropout model directly on screen. `Environment/DisasterVisualEffects.cs` renders animated water, fire,
smoke, rain, rain mist, landslide debris, and structural damage from the same ground-truth labels. Rainfall
also increases exponential-squared fog, cools the fog color, and reduces sunlight; wind controls rain and mist
direction. Debug waypoint and
anomaly meshes are hidden, while their transforms remain available to the agent. The ML-Agents policy receives
25 normalized vector observations, including all four disaster severities and weather state. No Asset Store
download is required for the B4/B5 workflow.

## First-time setup
1. In Unity Hub create a **3D (URP)** project in an empty temp folder (Unity 6 LTS or your team's pinned version).
2. Copy its `Packages/` and `ProjectSettings/` into this folder, then commit them. Pin the version in `ProjectSettings/ProjectVersion.txt`.
3. Package Manager -> add `com.unity.nuget.newtonsoft-json`.
4. Open `Assets/AeroSentinel/Scenes/Training 1.unity`. It is the converted mountain-terrain training scene:
   the existing terrain, trees, rocks, camera, and lighting are retained, while the AeroSentinel controller
   adds the dock, patrol waypoints, disaster scenario, drone, and virtual sensors at runtime.
5. Press Play, then from the repo root: `python scripts/run_unity_demo.py`.

The C# scripts were written without a Unity compile pass; expect a few small fixes on first import.
