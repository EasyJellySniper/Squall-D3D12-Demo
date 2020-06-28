using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// sq light
/// </summary>
[RequireComponent(typeof(Light))]
public class SqLight : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern int AddNativeLight(int _instanceID, SqLightData _sqLightData);

    [DllImport("SquallGraphics")]
    static extern void UpdateNativeLight(int _nativeID, SqLightData _sqLightData);

    [StructLayout(LayoutKind.Sequential)]
    struct SqLightData
    {
        /// <summary>
        /// color
        /// </summary>
        public Vector4 color;

        /// <summary>
        /// world dir as directional light, world pos for point/spotlight
        /// </summary>
        public Vector4 worldPos;

        /// <summary>
        /// type
        /// </summary>
        public int type;

        /// <summary>
        /// intensity
        /// </summary>
        public float intensity;

        /// <summary>
        /// padding
        /// </summary>
        public Vector2 padding;
    }

    SqLightData lightData;
    Light lightCache;
    int nativeID = -1;

    void Start()
    {
        // return if sqgraphic not init
        if (SqGraphicManager.instance == null)
        {
            enabled = false;
            return;
        }

        lightCache = GetComponent<Light>();
        lightData = new SqLightData();
        lightData.color = lightCache.color.linear;
        lightData.type = (int)lightCache.type;
        lightData.intensity = lightCache.intensity;

        if (lightCache.type == LightType.Directional)
        {
            lightData.worldPos = transform.forward;
        }
        else
        {
            lightData.worldPos = transform.position;
        }

        nativeID = AddNativeLight(lightCache.GetInstanceID(), lightData);
    }

    void Update()
    {
        if (transform.hasChanged || LightChanged())
        {
            if (lightCache.type == LightType.Directional)
            {
                lightData.worldPos = transform.forward;
            }
            else
            {
                lightData.worldPos = transform.position;
            }

            lightData.color = lightCache.color.linear;
            lightData.intensity = lightCache.intensity;

            UpdateNativeLight(nativeID, lightData);
            transform.hasChanged = false;
        }
    }

    bool LightChanged()
    {
        if (lightData.intensity != lightCache.intensity)
        {
            return true;
        }

        if (Vector4.SqrMagnitude(lightData.color - (Vector4)lightCache.color.linear) > 0.1)
        {
            return true;
        }

        return false;
    }
}
