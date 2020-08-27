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
        public float shadowSize;

        /// <summary>
        /// shadow distance
        /// </summary>
        public float shadowDistance;

        /// <summary>
        /// range from spot/point light
        /// </summary>
        public float range;

        /// <summary>
        /// padding
        /// </summary>
        public Vector3 padding;
    }

    /// <summary>
    /// shadow bias
    /// </summary>
    [Range(0.001f, 0.01f)]
    public float shadowBias = 0.003f;

    /// <summary>
    /// shadow distance
    /// </summary>
    public float shadowDistance = 500;

    SqLightData lightData;
    Light lightCache;
    Camera mainCam;
    Transform mainCamTrans;
    int nativeID = -1;

    void Start()
    {
        // return if sqgraphic not init
        if (SqGraphicManager.Instance == null)
        {
            enabled = false;
            return;
        }

        mainCam = Camera.main;
        mainCamTrans = mainCam.transform;
        InitNativeLight();
    }

    void Update()
    {
        UpdateNativeLight();
    }

    void InitNativeLight()
    {
        lightCache = GetComponent<Light>();
        lightData = new SqLightData();

        SetupLightData();
        nativeID = AddNativeLight(lightCache.GetInstanceID(), lightData);
    }

    void SetupLightData()
    {
        if (lightCache.type == LightType.Directional)
        {
            lightData.worldPos = transform.forward;
        }
        else
        {
            lightData.worldPos = transform.position;
        }

        lightData.type = (int)lightCache.type;
        lightData.color = lightCache.color.linear;
        lightData.color.w = lightCache.shadowStrength;
        lightData.intensity = lightCache.intensity;
        lightData.worldPos.w = shadowBias;
        lightData.range = (lightCache.type == LightType.Directional) ? float.MaxValue : lightCache.range;
        lightData.shadowDistance = shadowDistance;
        lightData.shadowSize = lightCache.cookieSize;
    }

    void UpdateNativeLight()
    {
        if (transform.hasChanged || LightChanged() || CascadeChanged() || (transform.parent && transform.parent.hasChanged))
        {
            SetupLightData();

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

        if (lightData.worldPos.w != shadowBias)
        {
            return true;
        }

        if (lightData.color.w != lightCache.shadowStrength)
        {
            return true;
        }

        if (lightData.shadowDistance != shadowDistance)
        {
            return true;
        }

        if (lightData.shadowSize != lightCache.cookieSize)
        {
            return true;
        }

        return false;
    }

    bool CascadeChanged()
    {
        if (lightCache.type != LightType.Directional)
        {
            return false;
        }

        return false;
    }
}
