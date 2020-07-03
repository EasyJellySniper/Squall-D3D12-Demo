using System;
using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// sq light
/// </summary>
[RequireComponent(typeof(Light))]
public class SqLight : MonoBehaviour
{
    public enum ShadowSize
    {
        S256 = 0, S512, S1024, S2048, S4096, S8192
    }

    [DllImport("SquallGraphics")]
    static extern int AddNativeLight(int _instanceID, SqLightData _sqLightData);

    [DllImport("SquallGraphics")]
    static extern void UpdateNativeLight(int _nativeID, SqLightData _sqLightData);

    [DllImport("SquallGraphics")]
    static extern void InitNativeShadows(int _nativeID, int _numCascade, IntPtr[] _shadowMaps);

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

    /// <summary>
    /// shadow size
    /// </summary>
    [Header("Shadow size, note even cascade use this size!")]
    public ShadowSize shadowSize = ShadowSize.S4096;

    /// <summary>
    /// cascade setting
    /// </summary>
    [Header("Cascade setting")]
    public float[] cascadeSetting;

    /// <summary>
    /// shadow map
    /// </summary>
    public RenderTexture[] shadowMaps;

    SqLightData lightData;
    Light lightCache;
    int nativeID = -1;
    int[] shadowMapSize = { 256, 512, 1024, 2048, 4096, 8192 };

    void Start()
    {
        // return if sqgraphic not init
        if (SqGraphicManager.instance == null)
        {
            enabled = false;
            return;
        }

        InitNativeLight();
        InitShadows();
    }

    void Update()
    {
        UpdateNativeLight();
    }

    void OnDestroy()
    {
        for (int i = 0; i < shadowMaps.Length; i++)
        {
            if (shadowMaps[i])
            {
                shadowMaps[i].Release();
                DestroyImmediate(shadowMaps[i]);
            }
        }
    }

    void InitNativeLight()
    {
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

    void InitShadows()
    {
        if (lightCache.shadows == LightShadows.None || lightCache.type != LightType.Directional)
        {
            return;
        }

        int size = shadowMapSize[(int)shadowSize];

        // create cascade shadows
        if (cascadeSetting.Length > 0)
        {
            shadowMaps = new RenderTexture[cascadeSetting.Length];
            for (int i = 0; i < shadowMaps.Length; i++)
            {
                shadowMaps[i] = new RenderTexture(size, size, 32, RenderTextureFormat.Depth);
                shadowMaps[i].name = name + "_ShadowMap " + i;
            }
        }
        else
        {
            // otherwise create normal shadow maps
            shadowMaps = new RenderTexture[1];
            shadowMaps[0] = new RenderTexture(size, size, 32, RenderTextureFormat.Depth);
            shadowMaps[0].name = name + "_ShadowMap";
        }

        IntPtr[] shadowPtr = new IntPtr[shadowMaps.Length];
        for (int i = 0; i < shadowMaps.Length; i++)
        {
            shadowPtr[i] = shadowMaps[i].GetNativeDepthBufferPtr();
        }
        InitNativeShadows(nativeID, shadowMaps.Length, shadowPtr);
    }

    void UpdateNativeLight()
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
