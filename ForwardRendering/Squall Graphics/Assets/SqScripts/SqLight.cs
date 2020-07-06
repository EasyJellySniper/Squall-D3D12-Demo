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
        /// shadow matrix
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.Struct, SizeConst = 4)]
        public Matrix4x4[] shadowMatrix;

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
        /// cascade
        /// </summary>
        public int numCascade;

        /// <summary>
        /// padding
        /// </summary>
        public float padding;
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
    Camera shadowCam;
    Camera mainCam;
    Transform mainCamTrans;

    int nativeID = -1;
    int[] shadowMapSize = { 256, 512, 1024, 2048, 4096, 8192 };
    float[] cascadeLast;

    void Start()
    {
        // return if sqgraphic not init
        if (SqGraphicManager.instance == null)
        {
            enabled = false;
            return;
        }

        mainCam = Camera.main;
        mainCamTrans = mainCam.transform;
        InitNativeLight();
        InitShadows();
    }

    void Update()
    {
        UpdateShadowMatrix();
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
        lightData.shadowMatrix = new Matrix4x4[4];
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

        if (cascadeSetting.Length > 4)
        {
            Debug.LogError("Max cascade is 4");
            return;
        }

        int size = shadowMapSize[(int)shadowSize];

        // create cascade shadows
        if (cascadeSetting.Length > 0)
        {
            shadowMaps = new RenderTexture[cascadeSetting.Length];
            cascadeLast = new float[cascadeSetting.Length];
            for (int i = 0; i < shadowMaps.Length; i++)
            {
                shadowMaps[i] = new RenderTexture(size, size, 32, RenderTextureFormat.Depth);
                shadowMaps[i].name = name + "_ShadowMap " + i;
                shadowMaps[i].Create();
                cascadeLast[i] = cascadeSetting[i];
            }
        }
        else
        {
            // otherwise create normal shadow maps
            shadowMaps = new RenderTexture[1];
            shadowMaps[0] = new RenderTexture(size, size, 32, RenderTextureFormat.Depth);
            shadowMaps[0].name = name + "_ShadowMap";
            shadowMaps[0].Create();
        }

        IntPtr[] shadowPtr = new IntPtr[shadowMaps.Length];
        for (int i = 0; i < shadowMaps.Length; i++)
        {
            shadowPtr[i] = shadowMaps[i].GetNativeDepthBufferPtr();
        }
        InitNativeShadows(nativeID, shadowMaps.Length, shadowPtr);

        // init shadow cam
        GameObject newObj = new GameObject();
        shadowCam = newObj.AddComponent<Camera>();
        shadowCam.transform.SetParent(lightCache.transform);
        shadowCam.transform.localPosition = Vector3.zero;
        shadowCam.transform.localRotation = Quaternion.identity;
        shadowCam.transform.localScale = Vector3.one;
        shadowCam.orthographic = true;
        shadowCam.targetDisplay = 3;
        shadowCam.aspect = 1f;
        transform.hasChanged = true;    // force update once
    }

    void UpdateShadowMatrix()
    {
        if (lightCache.type != LightType.Directional)
        {
            return;
        }

        if (transform.hasChanged || CascadeChanged())
        {
            for (int i = 0; i < cascadeSetting.Length; i++)
            {
                float dist = mainCam.farClipPlane * cascadeSetting[i];
                shadowCam.nearClipPlane = lightCache.shadowNearPlane;
                shadowCam.farClipPlane = dist;
                shadowCam.orthographicSize = dist;

                // position
                shadowCam.transform.position = mainCamTrans.position - lightCache.transform.forward * dist * 0.5f;
                lightData.shadowMatrix[i] = GL.GetGPUProjectionMatrix(shadowCam.projectionMatrix, true) * shadowCam.worldToCameraMatrix;
            }
            lightData.numCascade = cascadeSetting.Length;
        }

        for (int i = 0; i < cascadeSetting.Length; i++)
        {
            cascadeLast[i] = cascadeSetting[i];
        }
    }

    void UpdateNativeLight()
    {
        if (transform.hasChanged || LightChanged() || CascadeChanged())
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

    bool CascadeChanged()
    {
        if (lightCache.type != LightType.Directional)
        {
            return false;
        }

        for (int i = 0; i < cascadeSetting.Length; i++)
        {
            if (cascadeSetting[i] != cascadeLast[i])
            {
                return true;
            }
        }

        return false;
    }
}
