using System.Collections.Generic;
using UnityEngine;

namespace AeroSentinel
{
    public sealed class DisasterVisualEffects : MonoBehaviour
    {
        private DisasterScenarioEngine scenarioEngine;
        private SyntheticSensor sensor;
        private DisasterSource source;
        private Transform effectRoot;
        private GameObject water;
        private GameObject fireCore;
        private GameObject damageMarker;
        private ParticleSystem rain;
        private ParticleSystem rainMist;
        private ParticleSystem fire;
        private ParticleSystem smoke;
        private ParticleSystem debris;
        private readonly List<Transform> debrisPieces = new List<Transform>();
        private Vector3 effectCenter;
        private float groundY;
        private float windPhase;
        private readonly List<Transform> terrainElements = new List<Transform>();
        private readonly List<Vector3> terrainElementPositions = new List<Vector3>();
        private readonly List<Quaternion> terrainElementRotations = new List<Quaternion>();

        public void Bind(DisasterScenarioEngine engine, SyntheticSensor virtualSensor, DisasterSource disasterSource)
        {
            scenarioEngine = engine;
            sensor = virtualSensor;
            source = disasterSource;
            effectCenter = disasterSource.transform.position;
            groundY = effectCenter.y - 0.8f;
            CacheTerrainElements();
            effectRoot = new GameObject("DisasterVisualEffects").transform;
            effectRoot.SetParent(transform, true);
            CreateEffects();
        }

        private void Update()
        {
            if (scenarioEngine == null || sensor == null)
            {
                return;
            }

            if (water == null || fire == null || smoke == null || rain == null || rainMist == null || debris == null)
            {
                return;
            }

            GroundTruthLabel label = scenarioEngine.Evaluate(scenarioEngine.PrimaryScenario, scenarioEngine.SimulationTime);
            float severity = label.IsActive ? label.Severity : 0f;
            float radius = Mathf.Max(1f, label.Radius);
            windPhase += Time.deltaTime * Mathf.Lerp(1f, 8f, sensor.WindSpeedMetersPerSecond / 40f);

            if (water != null)
            {
                UpdateWater(label, severity, radius);
            }
            UpdateFire(label, severity, radius);
            UpdateDamage(label, severity, radius);
            UpdateRain(severity);
            UpdateDebris(label, severity);
            AnimateTerrain(label, severity);
            AnimateWind(severity);
        }

        private void CreateEffects()
        {
            water = CreatePrimitive("FloodWaterEffect", PrimitiveType.Cube, new Color(0.03f, 0.25f, 0.85f, 0.7f));
            fireCore = CreatePrimitive("FireGlowEffect", PrimitiveType.Sphere, new Color(1f, 0.18f, 0.02f, 0.9f));
            damageMarker = CreatePrimitive("StructuralDamageEffect", PrimitiveType.Cube, new Color(0.55f, 0.08f, 0.03f, 0.8f));
            water.SetActive(false);
            fireCore.SetActive(false);
            damageMarker.SetActive(false);
            fire = CreateParticles("FireParticles", new Color(1f, 0.22f, 0.02f, 0.95f), 90, 2.2f, 3.5f, 1.1f);
            smoke = CreateParticles("SmokeParticles", new Color(0.12f, 0.12f, 0.12f, 0.55f), 45, 4f, 5f, 2.5f);
            debris = CreateParticles("LandslideDebris", new Color(0.28f, 0.16f, 0.08f, 1f), 70, 1.4f, 2.2f, 4f);
            rain = CreateParticles("RainfallParticles", new Color(0.35f, 0.65f, 1f, 0.65f), 500, 8f, 10f, 0.6f);
            rainMist = CreateParticles("RainMistParticles", new Color(0.65f, 0.75f, 0.8f, 0.18f), 180, 5f, 3f, 0.2f);
            ConfigureRain();

            for (int index = 0; index < 8; index++)
            {
                GameObject piece = CreatePrimitive("DamageDebris_" + index, PrimitiveType.Cube, new Color(0.22f, 0.12f, 0.08f, 1f));
                piece.SetActive(false);
                debrisPieces.Add(piece.transform);
            }
        }

        private GameObject CreatePrimitive(string name, PrimitiveType type, Color color)
        {
            GameObject item = GameObject.CreatePrimitive(type);
            item.name = name;
            item.transform.SetParent(effectRoot, true);
            item.GetComponent<Collider>().enabled = false;
            Shader shader = Shader.Find("Universal Render Pipeline/Lit");
            if (shader == null)
            {
                shader = Shader.Find("Standard");
            }
            Material material = new Material(shader);
            material.color = color;
            material.EnableKeyword("_EMISSION");
            material.SetColor("_EmissionColor", color * 1.5f);
            item.GetComponent<Renderer>().material = material;
            return item;
        }

        private ParticleSystem CreateParticles(string name, Color color, int maxParticles, float lifetime, float size, float speed)
        {
            GameObject item = new GameObject(name);
            item.transform.SetParent(effectRoot, true);
            ParticleSystem particles = item.AddComponent<ParticleSystem>();
            ParticleSystem.MainModule main = particles.main;
            main.maxParticles = maxParticles;
            main.startLifetime = lifetime;
            main.startSpeed = speed;
            main.startSize = size;
            main.startColor = color;
            main.simulationSpace = ParticleSystemSimulationSpace.World;
            ParticleSystem.EmissionModule emission = particles.emission;
            emission.rateOverTime = maxParticles / Mathf.Max(1f, lifetime);
            ParticleSystem.ShapeModule shape = particles.shape;
            shape.shapeType = ParticleSystemShapeType.Circle;
            shape.radius = 2f;
            ParticleSystemRenderer renderer = particles.GetComponent<ParticleSystemRenderer>();
            renderer.renderMode = ParticleSystemRenderMode.Billboard;
            Shader shader = Shader.Find("Universal Render Pipeline/Particles/Unlit");
            if (shader == null)
            {
                shader = Shader.Find("Particles/Standard Unlit");
            }
            Material material = new Material(shader);
            material.color = color;
            renderer.material = material;
            return particles;
        }

        private void ConfigureRain()
        {
            ParticleSystem.MainModule main = rain.main;
            main.startLifetime = 1.3f;
            main.startSpeed = 18f;
            main.startSize = 0.035f;
            ParticleSystem.EmissionModule emission = rain.emission;
            emission.rateOverTime = 0f;
            ParticleSystem.ShapeModule shape = rain.shape;
            shape.shapeType = ParticleSystemShapeType.Box;
            shape.scale = new Vector3(25f, 0.1f, 25f);
            ParticleSystem.VelocityOverLifetimeModule velocity = rain.velocityOverLifetime;
            velocity.enabled = true;
            velocity.space = ParticleSystemSimulationSpace.World;
            rainMist.gameObject.SetActive(false);
            ParticleSystem.MainModule mistMain = rainMist.main;
            mistMain.startLifetime = 5f;
            mistMain.startSpeed = 0.2f;
            mistMain.startSize = 3f;
            ParticleSystem.EmissionModule mistEmission = rainMist.emission;
            mistEmission.rateOverTime = 0f;
            ParticleSystem.ShapeModule mistShape = rainMist.shape;
            mistShape.shapeType = ParticleSystemShapeType.Box;
            mistShape.scale = new Vector3(25f, 3f, 25f);
        }

        private void UpdateWater(GroundTruthLabel label, float severity, float radius)
        {
            bool active = label.Type == DisasterType.Flood && severity > 0.01f;
            water.SetActive(active);
            if (!active)
            {
                return;
            }

            float depth = Mathf.Max(0.05f, label.WaterDepth);
            water.transform.position = new Vector3(label.Center.x, groundY + depth * 0.35f, label.Center.z);
            water.transform.localScale = new Vector3(radius * 2f, depth * 0.7f, radius * 2f);
            water.transform.Rotate(Vector3.up, Time.deltaTime * (8f + sensor.WindSpeedMetersPerSecond));
        }

        private void UpdateFire(GroundTruthLabel label, float severity, float radius)
        {
            bool active = label.Type == DisasterType.Fire && severity > 0.01f;
            fire.gameObject.SetActive(active);
            smoke.gameObject.SetActive(active);
            fireCore.SetActive(active);
            if (!active)
            {
                return;
            }

            Vector3 center = label.Center + Vector3.up * 0.8f;
            fire.transform.position = center;
            smoke.transform.position = center + Vector3.up * 1.2f;
            fireCore.transform.position = center;
            fireCore.transform.localScale = Vector3.one * Mathf.Lerp(0.3f, 2.5f, severity);
            fire.transform.localScale = Vector3.one * Mathf.Lerp(0.5f, radius / 3f, severity);
            smoke.transform.localScale = Vector3.one * Mathf.Lerp(0.7f, 2.5f, severity);
        }

        private void UpdateDamage(GroundTruthLabel label, float severity, float radius)
        {
            bool active = label.Type == DisasterType.StructuralDamage && severity > 0.01f;
            damageMarker.SetActive(active);
            for (int index = 0; index < debrisPieces.Count; index++)
            {
                debrisPieces[index].gameObject.SetActive(active);
            }

            if (!active)
            {
                return;
            }

            damageMarker.transform.position = label.Center + Vector3.up * Mathf.Lerp(0.2f, 1.2f, severity);
            damageMarker.transform.localScale = new Vector3(radius, Mathf.Lerp(0.3f, 2f, severity), radius);
            damageMarker.transform.rotation = Quaternion.Euler(0f, windPhase * 10f, Mathf.Lerp(0f, 25f, severity));
        }

        private void UpdateRain(float severity)
        {
            float rainfall = Mathf.Clamp01(sensor.RainfallMmPerHour / 60f);
            bool active = rainfall > 0.005f;
            rain.gameObject.SetActive(active);
            rainMist.gameObject.SetActive(active);
            if (!active)
            {
                return;
            }

            rain.transform.position = effectCenter + Vector3.up * 14f;
            ParticleSystem.EmissionModule emission = rain.emission;
            emission.rateOverTime = Mathf.Lerp(15f, 560f, rainfall);
            ParticleSystem.VelocityOverLifetimeModule velocity = rain.velocityOverLifetime;
            velocity.x = sensor.WindSpeedMetersPerSecond * 0.8f;
            velocity.z = Mathf.Sin(windPhase) * sensor.WindSpeedMetersPerSecond * 0.25f;
            rain.transform.localScale = Vector3.one * Mathf.Lerp(0.75f, 1.35f, rainfall);
            rainMist.transform.position = effectCenter + Vector3.up * 5f;
            rainMist.transform.localScale = Vector3.one * Mathf.Lerp(0.8f, 1.6f, rainfall);
            ParticleSystem.EmissionModule mistEmission = rainMist.emission;
            mistEmission.rateOverTime = Mathf.Lerp(4f, 110f, rainfall);
            ParticleSystem.VelocityOverLifetimeModule mistVelocity = rainMist.velocityOverLifetime;
            mistVelocity.enabled = true;
            mistVelocity.x = sensor.WindSpeedMetersPerSecond * 0.12f;
            mistVelocity.z = Mathf.Sin(windPhase) * sensor.WindSpeedMetersPerSecond * 0.08f;
        }

        private void UpdateDebris(GroundTruthLabel label, float severity)
        {
            bool active = label.Type == DisasterType.Landslide && severity > 0.01f;
            debris.gameObject.SetActive(active);
            if (!active)
            {
                return;
            }

            debris.transform.position = label.Center + Vector3.up * 0.8f;
            debris.transform.localScale = Vector3.one * Mathf.Lerp(0.5f, 2f, severity);
            ParticleSystem.VelocityOverLifetimeModule velocity = debris.velocityOverLifetime;
            velocity.enabled = true;
            velocity.x = Mathf.Sin(windPhase) * severity * 2f;
            velocity.y = -Mathf.Lerp(0.5f, 4f, severity);
            velocity.z = Mathf.Cos(windPhase) * severity * 2f;
        }

        private void AnimateWind(float severity)
        {
            for (int index = 0; index < debrisPieces.Count; index++)
            {
                if (!debrisPieces[index].gameObject.activeSelf)
                {
                    continue;
                }

                float angle = index * 45f + windPhase * 20f;
                Vector3 offset = Quaternion.Euler(0f, angle, 0f) * Vector3.forward * (1f + severity * 2f);
                debrisPieces[index].position = effectCenter + offset + Vector3.up * (0.3f + Mathf.Abs(Mathf.Sin(windPhase + index)) * severity);
                debrisPieces[index].localScale = Vector3.one * (0.25f + severity * 0.6f);
                debrisPieces[index].Rotate(Vector3.one, Time.deltaTime * 40f);
            }
        }

        private void CacheTerrainElements()
        {
            GameObject terrain = GameObject.Find("mountain_terrain_01");
            if (terrain == null)
            {
                return;
            }

            Transform[] elements = terrain.GetComponentsInChildren<Transform>(true);
            foreach (Transform element in elements)
            {
                string name = element.name.ToLowerInvariant();
                if (!name.Contains("tree") && !name.Contains("rock"))
                {
                    continue;
                }

                terrainElements.Add(element);
                terrainElementPositions.Add(element.position);
                terrainElementRotations.Add(element.rotation);
            }
        }

        private void AnimateTerrain(GroundTruthLabel label, float severity)
        {
            bool shaking = label.IsActive
                && (label.Type == DisasterType.Landslide
                    || label.Type == DisasterType.StructuralDamage
                    || label.Type == DisasterType.Earthquake);
            float shake = shaking ? severity : 0f;
            for (int index = 0; index < terrainElements.Count; index++)
            {
                Transform element = terrainElements[index];
                if (element == null)
                {
                    continue;
                }

                if (shake <= 0.001f)
                {
                    element.position = terrainElementPositions[index];
                    element.rotation = terrainElementRotations[index];
                    continue;
                }

                float phase = windPhase * 8f + index * 0.71f;
                Vector3 offset = new Vector3(Mathf.Sin(phase), 0f, Mathf.Cos(phase * 1.13f)) * shake * 0.08f;
                element.position = terrainElementPositions[index] + offset;
                element.rotation = terrainElementRotations[index] * Quaternion.Euler(
                    Mathf.Sin(phase) * shake * 2.5f,
                    0f,
                    Mathf.Cos(phase * 1.2f) * shake * 2.5f);
            }
        }
    }
}
