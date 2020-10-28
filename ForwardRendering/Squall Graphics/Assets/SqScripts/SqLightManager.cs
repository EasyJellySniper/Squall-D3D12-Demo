using System;
using System.Runtime.InteropServices;
using UnityEngine;

// for init sq light
public class SqLightManager : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern void InitSqLight(int _numDirLight, int _numPointLight, int _numSpotLight);

    [DllImport("SquallGraphics")]
    static extern void InitRayShadow(IntPtr _collectShadows);

    [DllImport("SquallGraphics")]
    static extern void InitRayReflection(IntPtr _reflectionRT);

    [DllImport("SquallGraphics")]
    static extern void InitRayAmbient(IntPtr _ambientRT, IntPtr _noiseTex);

    [DllImport("SquallGraphics")]
    static extern void SetRayDistance(float _reflectionDistance);

    [DllImport("SquallGraphics")]
    static extern void SetPCFKernel(int _kernel);

    [DllImport("SquallGraphics")]
    static extern void SetAmbientSampleCount(int _count);

    [Header("Light Settings")]

    /// <summary>
    /// max dir light
    /// </summary>
    public int maxDirectionalLight = 5;

    /// <summary>
    /// max point light
    /// </summary>
    public int maxPointLight = 64;

    /// <summary>
    /// max spot light
    /// </summary>
    public int maxSpotLight = 32;

    /// <summary>
    /// pcss kernel
    /// </summary>
    [Range(0, 5)]
    public int pcssKernel = 3;

    [Header("Reflection Settings")]

    /// <summary>
    /// reflection down sample
    /// </summary>
    [Range(0, 3)]
    public int reflectionDownSample = 0;

    /// <summary>
    /// reflection distance
    /// </summary>
    public float reflectionDistance = 500;


    [Header("Ambient Settings")]

    /// <summary>
    /// ambient down sample
    /// </summary>
    [Range(0, 3)]
    public int ambientDownSample = 0;

    /// <summary>
    /// ambient sample count
    /// </summary>
    [Range(4, 64)]
    public int ambientSampleCount = 16;

    /// <summary>
    /// noise
    /// </summary>
    public Texture2D ambientNoise;

    /// <summary>
    /// collect shadows
    /// </summary>
    [HideInInspector]
    public RenderTexture collectShadows;

    /// <summary>
    /// reflection RT
    /// </summary>
    [HideInInspector]
    public RenderTexture reflectionRT;

    /// <summary>
    /// ambient rt
    /// </summary>
    [HideInInspector]
    public RenderTexture ambientRT;

    /// <summary>
    /// instance
    /// </summary>
    public static SqLightManager Instace { get; private set; }

    int lastSampleCount;

    // Start is called before the first frame update
    void Start()
    {
        Instace = this;
        InitLights();
        SetPCF();
    }

    void Update()
    {
        // set distance data
        SetRayDistance(reflectionDistance);

        // update
        SetPCF();
        SetAmbientSample();
    }

    void OnDestroy()
    {
        SqUtility.SafeDestroyRT(ref collectShadows);
        SqUtility.SafeDestroyRT(ref reflectionRT);
        SqUtility.SafeDestroyRT(ref ambientRT);
    }

    void InitLights()
    {
        int downSample = 1;

        // create collect shadows 
        collectShadows = SqUtility.CreateRT(Screen.width >> downSample, Screen.height >> downSample, 0, RenderTextureFormat.ARGBHalf, "Collect Shadows");

        // create reflection rt
        int reflSize = Mathf.ClosestPowerOfTwo(Screen.width) >> reflectionDownSample;
        reflectionRT = SqUtility.CreateRT(reflSize, reflSize, 0, RenderTextureFormat.ARGB32, "Reflection RT", true, true);

        // create ambient rt
        int ambientSize = Mathf.ClosestPowerOfTwo(Screen.width) >> ambientDownSample;
        ambientRT = SqUtility.CreateRT(ambientSize, ambientSize, 0, RenderTextureFormat.ARGB32, "Ambient RT", true);

        InitSqLight(maxDirectionalLight, maxPointLight, maxSpotLight);
        InitRayShadow(collectShadows.GetNativeTexturePtr());
        InitRayReflection(reflectionRT.GetNativeTexturePtr());
        InitRayAmbient(ambientRT.GetNativeTexturePtr(), ambientNoise.GetNativeTexturePtr());
    }

    void SetPCF()
    {
        SetPCFKernel(pcssKernel);
    }

    void SetAmbientSample()
    {
        if (lastSampleCount != ambientSampleCount)
        {
            SetAmbientSampleCount(ambientSampleCount);
        }

        lastSampleCount = ambientSampleCount;
    }
}
