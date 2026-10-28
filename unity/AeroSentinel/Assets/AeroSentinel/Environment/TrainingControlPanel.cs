using UnityEngine;

namespace AeroSentinel
{
    public sealed class TrainingControlPanel : MonoBehaviour
    {
        private DisasterScenarioEngine scenarioEngine;
        private SyntheticSensor sensor;
        private DisasterSource source;
        private GameObject drone;
        private GameObject terrain;
        private Light sun;
        private Vector2 scroll;
        private DisasterType disasterType = DisasterType.Fire;
        private float severity = 0.85f;
        private float radius = 4f;
        private float expansionRate = 0.04f;
        private float duration = 120f;
        private float timeScale = 1f;
        private float fog = 0.33f;
        private float terrainHeight = 1f;
        private bool panelVisible = true;
        private bool autoAdvance = true;
        private GUIStyle titleStyle;
        private GUIStyle sectionStyle;
        private GUIStyle labelStyle;
        private GUIStyle buttonStyle;
        private GUIStyle boxStyle;

        public void Bind(DisasterScenarioEngine engine, SyntheticSensor virtualSensor, DisasterSource disasterSource, GameObject simulatedDrone)
        {
            scenarioEngine = engine;
            sensor = virtualSensor;
            source = disasterSource;
            drone = simulatedDrone;
            terrain = GameObject.Find("mountain_terrain_01");
            sun = FindFirstObjectByType<Light>();
            ApplySettings();
        }

        private void Update()
        {
            if (scenarioEngine == null)
            {
                return;
            }

            Time.timeScale = timeScale;
            ApplySettings();
        }

        private void OnGUI()
        {
            EnsureStyles();
            if (!panelVisible)
            {
                if (GUI.Button(new Rect(20f, 20f, 180f, 44f), "SHOW CONTROLS", buttonStyle))
                {
                    panelVisible = true;
                }

                return;
            }

            float width = Mathf.Clamp(Screen.width * 0.31f, 380f, 500f);
            GUILayout.BeginArea(new Rect(20f, 20f, width, Screen.height - 40f), boxStyle);
            scroll = GUILayout.BeginScrollView(scroll);
            GUILayout.Label("AeroSentinel B4 / B5", titleStyle);
            GUILayout.Label("DISASTER + SENSOR CONTROL PANEL", sectionStyle);

            if (GUILayout.Button("HIDE CONTROLS", buttonStyle))
            {
                panelVisible = false;
            }

            GUILayout.Space(8f);
            GUILayout.Label("DISASTER SCENARIO", sectionStyle);
            GUILayout.Label("Type: " + disasterType, labelStyle);
            if (GUILayout.Button("CYCLE DISASTER TYPE", buttonStyle))
            {
                disasterType = (DisasterType)(((int)disasterType + 1) % 5);
            }

            severity = Slider("Severity", severity, 0f, 1f);
            radius = Slider("Affected radius (m)", radius, 0f, 30f);
            expansionRate = Slider("Expansion rate (m/s)", expansionRate, 0f, 2f);
            duration = Slider("Duration (s)", duration, 5f, 600f);
            timeScale = Slider("Simulation speed", timeScale, 0f, 5f);

            GUILayout.Space(8f);
            GUILayout.Label("WEATHER + TERRAIN", sectionStyle);
            sensor.RainfallMmPerHour = Slider("Rainfall (mm/h)", sensor.RainfallMmPerHour, 0f, 60f);
            sensor.AmbientTemperatureCelsius = Slider("Temperature (C)", sensor.AmbientTemperatureCelsius, -20f, 60f);
            sensor.WindSpeedMetersPerSecond = Slider("Wind speed (m/s)", sensor.WindSpeedMetersPerSecond, 0f, 40f);
            fog = Slider("Fog density", fog, 0f, 1f);
            terrainHeight = Slider("Terrain vertical scale", terrainHeight, 0.5f, 2f);

            GUILayout.Space(8f);
            GUILayout.Label("SENSOR NOISE / DROPOUT", sectionStyle);
            SetNoise(sensor.RgbNoise, "RGB");
            SetNoise(sensor.ThermalNoise, "Thermal");
            SetNoise(sensor.WaterLevelNoise, "Water level");
            SetNoise(sensor.RainfallNoise, "Rainfall");
            SetNoise(sensor.SoilMoistureNoise, "Soil moisture");
            SetNoise(sensor.WeatherNoise, "Weather");

            autoAdvance = GUILayout.Toggle(autoAdvance, " Run simulation automatically", labelStyle);
            if (GUILayout.Button("APPLY AND RESET", buttonStyle))
            {
                scenarioEngine.ResetSimulation();
            }

            if (GUILayout.Button("ADVANCE 10 SECONDS", buttonStyle))
            {
                scenarioEngine.Advance(10f);
            }

            GUILayout.Label("Simulation time: " + scenarioEngine.SimulationTime.ToString("0.0") + " s", labelStyle);
            GUILayout.Label("Ground-truth samples: " + scenarioEngine.GroundTruthLog.Count, labelStyle);
            GUILayout.EndScrollView();
            GUILayout.EndArea();
        }

        private float Slider(string label, float value, float minimum, float maximum)
        {
            GUILayout.Label(label + ": " + value.ToString("0.00"), labelStyle);
            return GUILayout.HorizontalSlider(value, minimum, maximum);
        }

        private void SetNoise(SensorNoiseModel noise, string label)
        {
            noise.StandardDeviation = Slider(label + " sigma", noise.StandardDeviation, 0f, 1f);
            noise.DropoutProbability = Slider(label + " dropout", noise.DropoutProbability, 0f, 1f);
        }

        private void ApplySettings()
        {
            if (scenarioEngine == null || sensor == null)
            {
                return;
            }

            scenarioEngine.SetAutoAdvance(autoAdvance);
            scenarioEngine.ConfigurePrimaryScenario(disasterType, severity, radius, expansionRate, duration);
            source.Type = disasterType;
            source.Severity = severity;
            source.Radius = radius;
            source.IsActive = severity > 0f;

            if (terrain != null)
            {
                Vector3 scale = terrain.transform.localScale;
                scale.y = terrainHeight;
                terrain.transform.localScale = scale;
            }

            float rainfallMist = Mathf.Clamp01(sensor.RainfallMmPerHour / 60f) * 0.018f;
            RenderSettings.fog = fog > 0.001f || rainfallMist > 0.001f;
            RenderSettings.fogMode = FogMode.ExponentialSquared;
            RenderSettings.fogDensity = Mathf.Clamp(fog * 0.018f + rainfallMist, 0f, 0.08f);
            RenderSettings.fogColor = Color.Lerp(
                new Color(0.55f, 0.7f, 0.8f),
                new Color(0.25f, 0.34f, 0.4f),
                Mathf.Clamp01(fog + rainfallMist * 18f));
            if (sun != null)
            {
                float weatherOcclusion = Mathf.Clamp01(fog + rainfallMist * 18f);
                sun.intensity = Mathf.Lerp(0.45f, 1.35f, 1f - weatherOcclusion);
            }
        }

        private void EnsureStyles()
        {
            if (titleStyle != null)
            {
                return;
            }

            boxStyle = new GUIStyle(GUI.skin.box)
            {
                padding = new RectOffset(18, 18, 16, 16),
                normal = { background = MakeTexture(new Color(0.025f, 0.04f, 0.07f, 0.94f)) }
            };
            titleStyle = new GUIStyle(GUI.skin.label)
            {
                fontSize = 23,
                fontStyle = FontStyle.Bold,
                normal = { textColor = Color.white }
            };
            sectionStyle = new GUIStyle(GUI.skin.label)
            {
                fontSize = 16,
                fontStyle = FontStyle.Bold,
                normal = { textColor = new Color(0.35f, 0.85f, 1f) }
            };
            labelStyle = new GUIStyle(GUI.skin.label)
            {
                fontSize = 14,
                normal = { textColor = Color.white },
                wordWrap = true
            };
            buttonStyle = new GUIStyle(GUI.skin.button)
            {
                fontSize = 14,
                fontStyle = FontStyle.Bold,
                fixedHeight = 36
            };
        }

        private static Texture2D MakeTexture(Color color)
        {
            Texture2D texture = new Texture2D(1, 1);
            texture.SetPixel(0, 0, color);
            texture.Apply();
            return texture;
        }

    }
}
