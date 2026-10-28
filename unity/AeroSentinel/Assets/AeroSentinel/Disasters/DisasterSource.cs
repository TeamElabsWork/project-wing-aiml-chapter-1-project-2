using UnityEngine;

namespace AeroSentinel
{
    public sealed class DisasterSource : MonoBehaviour
    {
        [Range(0f, 1f)] public float Severity = 0.85f;
        public bool IsActive = true;
        public float ActivationTime;
        public DisasterType Type = DisasterType.Fire;
        public float Radius = 2f;

        public float GetAnomalyStrength(Vector3 observerPosition)
        {
            if (!IsActive)
            {
                return 0f;
            }

            float distance = Vector3.Distance(observerPosition, transform.position);
            float spatialFalloff = 1f - Mathf.Clamp01(distance / Mathf.Max(0.01f, Radius));
            return Mathf.Clamp01(Severity * spatialFalloff);
        }
    }
}
