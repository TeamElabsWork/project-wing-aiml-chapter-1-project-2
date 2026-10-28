using Unity.MLAgents;
using Unity.MLAgents.Actuators;
using Unity.MLAgents.Sensors;
using UnityEngine;

namespace AeroSentinel
{
    public sealed class AeroSentinelAgent : Agent
    {
        public SimulatedDroneController Controller { get; set; }
        public DisasterSource Anomaly { get; set; }
        public PatrolWaypoint[] Waypoints { get; set; }

        private SyntheticSensor sensor;
        private int nextWaypoint;
        private bool investigated;

        public override void Initialize()
        {
            sensor = GetComponent<SyntheticSensor>();
        }

        public override void OnEpisodeBegin()
        {
            Controller.ResetDrone();
            nextWaypoint = 0;
            investigated = false;
        }

        public override void CollectObservations(VectorSensor observation)
        {
            sensor.Read(Anomaly);
            Vector3 relativeDock = Controller.Dock == null ? Vector3.zero : Controller.Dock.transform.position - Controller.Position;
            PatrolWaypoint waypoint = Waypoints != null && Waypoints.Length > 0 ? Waypoints[nextWaypoint % Waypoints.Length] : null;
            Vector3 relativeWaypoint = waypoint == null ? Vector3.zero : waypoint.transform.position - Controller.Position;

            observation.AddObservation(Controller.Position / 20f);
            observation.AddObservation(Controller.Velocity / 10f);
            observation.AddObservation(Controller.Battery / 100f);
            observation.AddObservation(relativeDock / 20f);
            observation.AddObservation(relativeWaypoint / 20f);
            observation.AddObservation(sensor.Temperature);
            observation.AddObservation(sensor.Smoke);
            observation.AddObservation(sensor.Humidity);
            observation.AddObservation(Anomaly == null ? 0f : Anomaly.GetAnomalyStrength(Controller.Position));
            observation.AddObservation(investigated ? 1f : 0f);
            observation.AddObservation(sensor.GetGroundTruth(DisasterType.Fire).Severity);
            observation.AddObservation(sensor.GetGroundTruth(DisasterType.Flood).Severity);
            observation.AddObservation(sensor.GetGroundTruth(DisasterType.Landslide).Severity);
            observation.AddObservation(sensor.GetGroundTruth(DisasterType.StructuralDamage).Severity);
            observation.AddObservation(sensor.GetGroundTruth(DisasterType.Earthquake).Severity);
            observation.AddObservation(Mathf.Clamp01(sensor.RainfallMmPerHour / 60f));
            observation.AddObservation(Mathf.Clamp01(sensor.WindSpeedMetersPerSecond / 40f));
            observation.AddObservation(Mathf.Clamp01(sensor.AmbientTemperatureCelsius / 60f));
        }

        public override void OnActionReceived(ActionBuffers actions)
        {
            if (Controller == null || actions.DiscreteActions.Length == 0)
            {
                return;
            }

            int action = actions.DiscreteActions[0];
            switch (action)
            {
                case 0:
                    Controller.TakeOff();
                    AddReward(-0.001f);
                    break;
                case 1:
                    if (Anomaly == null)
                    {
                        AddReward(-0.01f);
                        break;
                    }

                    Controller.MoveTo(Anomaly.transform.position + Vector3.up * 1.5f);
                    if (Anomaly.GetAnomalyStrength(Controller.Position) > 0.7f)
                    {
                        investigated = true;
                        AddReward(0.2f);
                    }
                    break;
                case 2:
                    if (Waypoints != null && Waypoints.Length > 0)
                    {
                        Controller.MoveTo(Waypoints[nextWaypoint % Waypoints.Length].transform.position);
                        nextWaypoint = (nextWaypoint + 1) % Waypoints.Length;
                        AddReward(0.02f);
                    }
                    break;
                case 3:
                    Controller.ReturnToDock();
                    AddReward(Controller.Battery < Controller.MinimumReturnBattery ? 0.1f : -0.01f);
                    break;
                case 4:
                    Controller.Recharge();
                    AddReward(Controller.IsAtDock ? 0.05f : -0.02f);
                    break;
            }

            if (investigated && Controller.IsAtDock)
            {
                AddReward(0.5f);
                EndEpisode();
            }

            if (Controller.Battery <= 0f)
            {
                AddReward(-1f);
                EndEpisode();
            }
        }

        public override void Heuristic(in ActionBuffers actionsOut)
        {
            ActionSegment<int> actions = actionsOut.DiscreteActions;
            actions[0] = 0;
            if (Input.GetKey(KeyCode.I)) actions[0] = 1;
            if (Input.GetKey(KeyCode.W)) actions[0] = 2;
            if (Input.GetKey(KeyCode.R)) actions[0] = 3;
            if (Input.GetKey(KeyCode.C)) actions[0] = 4;
        }
    }
}
