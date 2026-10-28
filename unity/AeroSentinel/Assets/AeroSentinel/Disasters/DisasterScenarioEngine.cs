using System;
using System.Collections.Generic;
using UnityEngine;

namespace AeroSentinel
{
    public enum DisasterType
    {
        Flood,
        Fire,
        Landslide,
        StructuralDamage,
        Earthquake
    }

    [Serializable]
    public sealed class DisasterScenario
    {
        public string Id = "scenario-001";
        public DisasterType Type = DisasterType.Flood;
        public Vector3 Center;
        public float StartTime;
        public float Duration = 60f;
        [Range(0f, 1f)] public float PeakSeverity = 1f;
        public float Radius = 5f;
        public float ExpansionRate = 0.5f;
    }

    [Serializable]
    public sealed class GroundTruthLabel
    {
        public string ScenarioId;
        public DisasterType Type;
        public float Timestamp;
        public Vector3 Center;
        public float Radius;
        [Range(0f, 1f)] public float Severity;
        public bool IsActive;
        public float WaterDepth;
        public float ThermalIntensity;
        public float SoilFailure;
        public float StructuralDamage;
        public float EarthquakeIntensity;
    }

    public sealed class DisasterScenarioEngine : MonoBehaviour
    {
        [SerializeField] private List<DisasterScenario> scenarios = new List<DisasterScenario>();
        [SerializeField] private bool runOnStart = true;
        [SerializeField] private bool useUnityTime = true;
        [SerializeField] private float simulationTime;
        [SerializeField] private float fixedStep = 0.1f;
        [SerializeField] private List<GroundTruthLabel> groundTruthLog = new List<GroundTruthLabel>();
        private float accumulatedStep;

        public float SimulationTime => simulationTime;
        public IReadOnlyList<DisasterScenario> Scenarios => scenarios;
        public IReadOnlyList<GroundTruthLabel> GroundTruthLog => groundTruthLog;
        public event Action<IReadOnlyList<GroundTruthLabel>> FrameEvaluated;
        public bool UseUnityTime => useUnityTime;

        public void AddScenario(DisasterScenario scenario)
        {
            if (scenario == null)
            {
                throw new ArgumentNullException(nameof(scenario));
            }

            EnsureCollections();
            scenarios.Add(scenario);
        }

        public DisasterScenario PrimaryScenario
        {
            get
            {
                EnsureCollections();
                if (scenarios.Count == 0)
                {
                    AddScenario(new DisasterScenario());
                }

                return scenarios[0];
            }
        }

        public void SetAutoAdvance(bool enabled)
        {
            useUnityTime = enabled;
        }

        public void ConfigurePrimaryScenario(DisasterType type, float severity, float radius, float expansionRate, float duration)
        {
            DisasterScenario scenario = PrimaryScenario;
            bool changed = scenario.Type != type
                || !Mathf.Approximately(scenario.PeakSeverity, severity)
                || !Mathf.Approximately(scenario.Radius, radius)
                || !Mathf.Approximately(scenario.ExpansionRate, expansionRate)
                || !Mathf.Approximately(scenario.Duration, duration);
            scenario.Type = type;
            scenario.PeakSeverity = Mathf.Clamp01(severity);
            scenario.Radius = Mathf.Max(0f, radius);
            scenario.ExpansionRate = Mathf.Max(0f, expansionRate);
            scenario.Duration = Mathf.Max(0.01f, duration);
            if (changed)
            {
                EvaluateFrame();
            }
        }

        private void Start()
        {
            if (runOnStart)
            {
                EvaluateFrame();
            }
        }

        private void Update()
        {
            if (!useUnityTime)
            {
                return;
            }

            float step = Mathf.Max(0.001f, fixedStep);
            accumulatedStep += Time.deltaTime;
            while (accumulatedStep >= step)
            {
                accumulatedStep -= step;
                Advance(step);
            }
        }

        public void ResetSimulation(float time = 0f)
        {
            EnsureCollections();
            simulationTime = time;
            accumulatedStep = 0f;
            groundTruthLog.Clear();
            EvaluateFrame();
        }

        public void Advance(float seconds)
        {
            if (seconds < 0f)
            {
                throw new ArgumentOutOfRangeException(nameof(seconds), "Simulation time cannot move backwards.");
            }

            simulationTime += seconds;
            EvaluateFrame();
        }

        public List<GroundTruthLabel> EvaluateFrame()
        {
            EnsureCollections();
            var labels = new List<GroundTruthLabel>(scenarios.Count);
            foreach (DisasterScenario scenario in scenarios)
            {
                labels.Add(Evaluate(scenario, simulationTime));
            }

            groundTruthLog.AddRange(labels);
            FrameEvaluated?.Invoke(labels);
            return labels;
        }

        public GroundTruthLabel Evaluate(DisasterScenario scenario, float time)
        {
            if (scenario == null)
            {
                throw new ArgumentNullException(nameof(scenario));
            }

            float elapsed = time - scenario.StartTime;
            bool active = elapsed >= 0f && elapsed <= Mathf.Max(0.01f, scenario.Duration);
            float progress = Mathf.Clamp01(elapsed / Mathf.Max(0.01f, scenario.Duration));
            float envelope = Mathf.Sin(progress * Mathf.PI);
            float severity = active ? Mathf.Clamp01(scenario.PeakSeverity * envelope) : 0f;
            float radius = active
                ? scenario.Radius + Mathf.Max(0f, scenario.ExpansionRate) * Mathf.Max(0f, elapsed)
                : scenario.Radius;

            var label = new GroundTruthLabel
            {
                ScenarioId = scenario.Id,
                Type = scenario.Type,
                Timestamp = time,
                Center = scenario.Center,
                Radius = radius,
                Severity = severity,
                IsActive = active
            };

            switch (scenario.Type)
            {
                case DisasterType.Flood:
                    label.WaterDepth = severity * 3f;
                    break;
                case DisasterType.Fire:
                    label.ThermalIntensity = severity;
                    break;
                case DisasterType.Landslide:
                    label.SoilFailure = severity;
                    break;
                case DisasterType.StructuralDamage:
                    label.StructuralDamage = severity;
                    break;
                case DisasterType.Earthquake:
                    label.EarthquakeIntensity = severity;
                    break;
            }

            return label;
        }

        public GroundTruthLabel GetGroundTruth(Vector3 position, DisasterType type)
        {
            EnsureCollections();
            GroundTruthLabel best = null;
            foreach (DisasterScenario scenario in scenarios)
            {
                GroundTruthLabel label = Evaluate(scenario, simulationTime);
                if (!label.IsActive || label.Type != type)
                {
                    continue;
                }

                float proximity = label.Radius <= 0f
                    ? (Vector3.Distance(position, label.Center) <= 0.001f ? 1f : 0f)
                    : 1f - Mathf.Clamp01(Vector3.Distance(position, label.Center) / label.Radius);
                label.Severity *= proximity;
                label.WaterDepth *= proximity;
                label.ThermalIntensity *= proximity;
                label.SoilFailure *= proximity;
                label.StructuralDamage *= proximity;
                label.EarthquakeIntensity *= proximity;
                if (best == null || label.Severity > best.Severity)
                {
                    best = label;
                }
            }

            return best ?? new GroundTruthLabel
            {
                Type = type,
                Timestamp = simulationTime,
                Center = position,
                IsActive = false
            };
        }

        private void EnsureCollections()
        {
            if (scenarios == null)
            {
                scenarios = new List<DisasterScenario>();
            }

            if (groundTruthLog == null)
            {
                groundTruthLog = new List<GroundTruthLabel>();
            }
        }
    }
}
