using System;
using UnityEngine;

namespace AeroSentinel
{
    public enum VirtualSensorType
    {
        RGB,
        Thermal,
        WaterLevel,
        Rainfall,
        SoilMoisture,
        Weather
    }

    [Serializable]
    public sealed class SensorNoiseModel
    {
        public float Bias;
        public float StandardDeviation = 0.01f;
        [Range(0f, 1f)] public float DropoutProbability;
    }

    [Serializable]
    public sealed class SensorReading
    {
        public VirtualSensorType Sensor;
        public float Timestamp;
        public bool Valid;
        public float Value;
        public float SecondaryValue;
        public float TertiaryValue;
        public float GroundTruth;
    }

    public sealed class SyntheticSensor : MonoBehaviour
    {
        public DisasterScenarioEngine ScenarioEngine;
        public int Seed = 7;
        public SensorNoiseModel RgbNoise = new SensorNoiseModel { StandardDeviation = 0.02f };
        public SensorNoiseModel ThermalNoise = new SensorNoiseModel { StandardDeviation = 0.03f };
        public SensorNoiseModel WaterLevelNoise = new SensorNoiseModel { StandardDeviation = 0.05f };
        public SensorNoiseModel RainfallNoise = new SensorNoiseModel { StandardDeviation = 0.5f };
        public SensorNoiseModel SoilMoistureNoise = new SensorNoiseModel { StandardDeviation = 0.02f };
        public SensorNoiseModel WeatherNoise = new SensorNoiseModel { StandardDeviation = 0.03f };
        [Range(0f, 60f)] public float RainfallMmPerHour = 8f;
        [Range(-20f, 60f)] public float AmbientTemperatureCelsius = 24f;
        [Range(0f, 40f)] public float WindSpeedMetersPerSecond = 3f;

        private System.Random random;

        public float Temperature { get; private set; }
        public float Smoke { get; private set; }
        public float Humidity { get; private set; }

        private void Awake()
        {
            random = new System.Random(Seed);
        }

        public SensorReading[] ReadSuite()
        {
            return new[]
            {
                Read(VirtualSensorType.RGB),
                Read(VirtualSensorType.Thermal),
                Read(VirtualSensorType.WaterLevel),
                Read(VirtualSensorType.Rainfall),
                Read(VirtualSensorType.SoilMoisture),
                Read(VirtualSensorType.Weather)
            };
        }

        public SensorReading Read(VirtualSensorType sensorType)
        {
            GroundTruthLabel fire = GroundTruth(DisasterType.Fire);
            GroundTruthLabel flood = GroundTruth(DisasterType.Flood);
            GroundTruthLabel landslide = GroundTruth(DisasterType.Landslide);
            GroundTruthLabel structural = GroundTruth(DisasterType.StructuralDamage);
            GroundTruthLabel earthquake = GroundTruth(DisasterType.Earthquake);
            float anomaly = Mathf.Clamp01(Mathf.Max(fire.Severity, flood.Severity, landslide.Severity, structural.Severity, earthquake.Severity));
            SensorNoiseModel noise = NoiseFor(sensorType);
            float truth;
            float secondary = 0f;
            float tertiary = 0f;

            switch (sensorType)
            {
                case VirtualSensorType.RGB:
                    truth = anomaly;
                    secondary = fire.Severity;
                    tertiary = flood.Severity;
                    break;
                case VirtualSensorType.Thermal:
                    truth = fire.ThermalIntensity;
                    break;
                case VirtualSensorType.WaterLevel:
                    truth = flood.WaterDepth;
                    break;
                case VirtualSensorType.Rainfall:
                    truth = Mathf.Max(0f, RainfallMmPerHour + flood.Severity * 18f);
                    break;
                case VirtualSensorType.SoilMoisture:
                    truth = Mathf.Clamp01(0.45f + flood.Severity * 0.4f + landslide.SoilFailure * 0.15f);
                    break;
                case VirtualSensorType.Weather:
                    truth = Mathf.Clamp01((AmbientTemperatureCelsius + 20f) / 80f - fire.Severity * 0.25f);
                    secondary = Mathf.Clamp01(WindSpeedMetersPerSecond / 40f + flood.Severity * 0.3f);
                    break;
                default:
                    throw new ArgumentOutOfRangeException(nameof(sensorType), sensorType, null);
            }

            bool valid = NextUnit() >= noise.DropoutProbability;
            float value = valid ? Mathf.Max(0f, truth + noise.Bias + Gaussian(noise.StandardDeviation)) : 0f;
            return new SensorReading
            {
                Sensor = sensorType,
                Timestamp = ScenarioEngine == null ? Time.time : ScenarioEngine.SimulationTime,
                Valid = valid,
                Value = value,
                SecondaryValue = valid ? secondary : 0f,
                TertiaryValue = valid ? tertiary : 0f,
                GroundTruth = truth
            };
        }

        public void Read(DisasterSource source)
        {
            float anomaly = source == null ? 0f : source.GetAnomalyStrength(transform.position);
            Temperature = Mathf.Clamp01(0.25f + anomaly * 0.75f);
            Smoke = Mathf.Clamp01(anomaly * 1.1f);
            Humidity = Mathf.Clamp01(0.6f - anomaly * 0.35f);
        }

        public GroundTruthLabel GetGroundTruth(DisasterType type)
        {
            return GroundTruth(type);
        }

        private GroundTruthLabel GroundTruth(DisasterType type)
        {
            return ScenarioEngine == null
                ? new GroundTruthLabel { Type = type, Center = transform.position }
                : ScenarioEngine.GetGroundTruth(transform.position, type);
        }

        private SensorNoiseModel NoiseFor(VirtualSensorType sensorType)
        {
            switch (sensorType)
            {
                case VirtualSensorType.RGB: return RgbNoise;
                case VirtualSensorType.Thermal: return ThermalNoise;
                case VirtualSensorType.WaterLevel: return WaterLevelNoise;
                case VirtualSensorType.Rainfall: return RainfallNoise;
                case VirtualSensorType.SoilMoisture: return SoilMoistureNoise;
                default: return WeatherNoise;
            }
        }

        private float NextUnit()
        {
            return (float)random.NextDouble();
        }

        private float Gaussian(float standardDeviation)
        {
            if (standardDeviation <= 0f)
            {
                return 0f;
            }

            float u1 = Mathf.Max(1e-6f, NextUnit());
            float u2 = NextUnit();
            return standardDeviation * Mathf.Sqrt(-2f * Mathf.Log(u1)) * Mathf.Cos(2f * Mathf.PI * u2);
        }
    }
}
