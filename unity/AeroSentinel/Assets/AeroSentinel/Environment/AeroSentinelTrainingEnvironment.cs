using UnityEngine;
using Unity.MLAgents;
using Unity.MLAgents.Actuators;
using Unity.MLAgents.Policies;

namespace AeroSentinel
{
    public sealed class AeroSentinelTrainingEnvironment : MonoBehaviour
    {
        [Header("Scene setup")]
        [SerializeField] private bool buildBaseEnvironment = true;
        [SerializeField] private bool useExistingMainCamera = true;

        private readonly Vector3[] waypointPositions =
        {
            new Vector3(0f, 1.5f, 9f),
            new Vector3(10f, 1.5f, 2f),
            new Vector3(-9f, 1.5f, -7f)
        };

        private AeroSentinelAgent agent;

        private void Awake()
        {
            BuildEnvironment();
        }

        private void BuildEnvironment()
        {
            if (buildBaseEnvironment)
            {
                if (!useExistingMainCamera)
                {
                    CreateCamera();
                }

                CreateLighting();
                CreatePrimitive("Ground", PrimitiveType.Cube, new Vector3(0f, -0.5f, 0f), new Vector3(28f, 1f, 28f), new Color(0.16f, 0.22f, 0.2f));
            }

            GameObject dockObject = CreatePrimitive("Dock", PrimitiveType.Cylinder, PlaceOnGround(new Vector3(0f, 0f, -10f), 0.15f), new Vector3(3f, 0.2f, 3f), new Color(0.15f, 0.6f, 0.8f));
            dockObject.GetComponent<Renderer>().enabled = false;
            DockingStation dock = dockObject.AddComponent<DockingStation>();

            for (int index = 0; index < waypointPositions.Length; index++)
            {
                GameObject waypointObject = CreatePrimitive("Waypoint_" + index, PrimitiveType.Cylinder, PlaceOnGround(waypointPositions[index], 0.15f), new Vector3(1.2f, 0.15f, 1.2f), new Color(0.9f, 0.75f, 0.2f));
                waypointObject.GetComponent<Renderer>().enabled = false;
                waypointObject.AddComponent<PatrolWaypoint>().Index = index;
            }

            GameObject anomalyObject = CreatePrimitive("SyntheticAnomaly", PrimitiveType.Sphere, PlaceOnGround(new Vector3(8f, 0f, 6f), 0.8f), new Vector3(2f, 1.6f, 2f), new Color(0.9f, 0.2f, 0.12f));
            anomalyObject.GetComponent<Renderer>().enabled = false;
            DisasterSource source = anomalyObject.AddComponent<DisasterSource>();
            source.Severity = 0.85f;

            GameObject scenarioObject = new GameObject("DisasterScenarioEngine");
            DisasterScenarioEngine scenarioEngine = scenarioObject.AddComponent<DisasterScenarioEngine>();
            scenarioEngine.AddScenario(new DisasterScenario
            {
                Id = "training-fire",
                Type = DisasterType.Fire,
                Center = anomalyObject.transform.position,
                StartTime = 0f,
                Duration = 120f,
                PeakSeverity = source.Severity,
                Radius = 4f,
                ExpansionRate = 0.04f
            });
            scenarioEngine.ResetSimulation();
            DisasterVisualEffects visualEffects = gameObject.GetComponent<DisasterVisualEffects>();
            if (visualEffects == null)
            {
                visualEffects = gameObject.AddComponent<DisasterVisualEffects>();
            }

            GameObject droneObject = CreatePrimitive("Drone", PrimitiveType.Sphere, dock.transform.position + Vector3.up * 1.5f, Vector3.one, new Color(0.85f, 0.9f, 0.95f));
            droneObject.GetComponent<Renderer>().enabled = false;
            droneObject.SetActive(false);
            SimulatedDroneController controller = droneObject.AddComponent<SimulatedDroneController>();
            controller.Dock = dock;
            controller.Waypoints = FindWaypoints();
            SyntheticSensor sensor = droneObject.AddComponent<SyntheticSensor>();
            sensor.ScenarioEngine = scenarioEngine;
            visualEffects.Bind(scenarioEngine, sensor, source);
            BehaviorParameters behavior = droneObject.AddComponent<BehaviorParameters>();
            behavior.BrainParameters.VectorObservationSize = 26;
            behavior.BrainParameters.ActionSpec = ActionSpec.MakeDiscrete(5);
            behavior.BehaviorType = BehaviorType.Default;
            behavior.BehaviorName = "AeroSentinelMissionPolicy";
            DecisionRequester requester = droneObject.AddComponent<DecisionRequester>();
            requester.DecisionPeriod = 5;
            agent = droneObject.AddComponent<AeroSentinelAgent>();
            agent.Controller = controller;
            agent.Anomaly = source;
            agent.Waypoints = controller.Waypoints;
            droneObject.SetActive(true);

            TrainingControlPanel panel = gameObject.GetComponent<TrainingControlPanel>();
            if (panel == null)
            {
                panel = gameObject.AddComponent<TrainingControlPanel>();
            }

            panel.Bind(scenarioEngine, sensor, source, droneObject);
        }

        private static void CreateCamera()
        {
            if (Camera.main != null)
            {
                return;
            }

            GameObject cameraObject = new GameObject("TrainingCamera");
            Camera camera = cameraObject.AddComponent<Camera>();
            cameraObject.tag = "MainCamera";
            cameraObject.transform.position = new Vector3(0f, 24f, -25f);
            cameraObject.transform.rotation = Quaternion.Euler(38f, 0f, 0f);
            camera.fieldOfView = 55f;
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(0.06f, 0.09f, 0.12f);
        }

        private static void CreateLighting()
        {
            GameObject lightObject = new GameObject("TrainingSun");
            Light light = lightObject.AddComponent<Light>();
            light.type = LightType.Directional;
            light.intensity = 1.2f;
            lightObject.transform.rotation = Quaternion.Euler(50f, -30f, 0f);
        }

        private PatrolWaypoint[] FindWaypoints()
        {
            return FindObjectsByType<PatrolWaypoint>();
        }

        private static Vector3 PlaceOnGround(Vector3 position, float height)
        {
            Vector3 rayOrigin = new Vector3(position.x, 200f, position.z);
            RaycastHit hit;
            if (Physics.Raycast(rayOrigin, Vector3.down, out hit, 400f))
            {
                position.y = hit.point.y + height;
            }
            else
            {
                position.y = height;
            }

            return position;
        }

        private static GameObject CreatePrimitive(string objectName, PrimitiveType primitiveType, Vector3 position, Vector3 scale, Color color)
        {
            GameObject item = GameObject.CreatePrimitive(primitiveType);
            item.name = objectName;
            item.transform.position = position;
            item.transform.localScale = scale;
            Renderer renderer = item.GetComponent<Renderer>();
            renderer.material.color = color;
            return item;
        }

        private void OnGUI()
        {
            if (agent == null || agent.Controller == null)
            {
                return;
            }

            GUI.Label(new Rect(16f, 16f, 500f, 24f), "AeroSentinel Training | Battery: " + agent.Controller.Battery.ToString("0.00") + " | State: " + agent.Controller.State);
            GUI.Label(new Rect(16f, 40f, 700f, 24f), "Actions: 0 Continue | 1 Investigate | 2 Next waypoint | 3 Return | 4 Recharge");
        }
    }
}
