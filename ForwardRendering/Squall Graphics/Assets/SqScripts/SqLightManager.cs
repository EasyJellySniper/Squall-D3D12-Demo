using System;
using System.Runtime.InteropServices;
using UnityEngine;

// for init sq light
public class SqLightManager : MonoBehaviour
{
    [Serializable]
    public struct AmbientConstant
    {
        public static bool operator ==(AmbientConstant lhs, AmbientConstant rhs)
        {
            return lhs.sampleCount == rhs.sampleCount
                && lhs.occlusionDist == rhs.occlusionDist
                && lhs.diffuseDist == rhs.diffuseDist
                && lhs.diffuseFadeDist == rhs.diffuseFadeDist
                && lhs.diffuseStrength == rhs.diffuseStrength
                && lhs.occlusionFadeDist == rhs.occlusionFadeDist
                && lhs.occlusionStrength == rhs.occlusionStrength
                && lhs.blurRadius == rhs.blurRadius
                && lhs.noiseTiling == rhs.noiseTiling
                && lhs.blurDepthThres == rhs.blurDepthThres
                && lhs.blurNormalThres == rhs.blurNormalThres;
        }

        public static bool operator !=(AmbientConstant lhs, AmbientConstant rhs)
        {
            return lhs.sampleCount != rhs.sampleCount
                || lhs.occlusionDist != rhs.occlusionDist
                || lhs.diffuseDist != rhs.diffuseDist
                || lhs.diffuseFadeDist != rhs.diffuseFadeDist
                || lhs.diffuseStrength != rhs.diffuseStrength
                || lhs.occlusionFadeDist != rhs.occlusionFadeDist
                || lhs.occlusionStrength != rhs.occlusionStrength
                || lhs.blurRadius != rhs.blurRadius
                || lhs.noiseTiling != rhs.noiseTiling
                || lhs.blurDepthThres != rhs.blurDepthThres
                || lhs.blurNormalThres != rhs.blurNormalThres;
        }

        public float diffuseDist;
        public float diffuseFadeDist;

        [Range(0, 8)]
        public float diffuseStrength;
        public float occlusionDist;
        public float occlusionFadeDist;

        [Range(0, 1)]
        public float occlusionStrength;
        public float noiseTiling;

        [Range(0, 1)]
        public float blurDepthThres;

        [Range(0, 1)]
        public float blurNormalThres;
        public int sampleCount;

        [HideInInspector]
        public int dummy;   // with other usage in native

        [Range(1, 7)]
        public int blurRadius;
    };

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
    static extern void SetAmbientData(AmbientConstant _ac);

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
    /// ambient const
    /// </summary>
    public AmbientConstant ambientConst;

    /// <summary>
    /// noise
    /// </summary>
    public Texture2D ambientNoise;

    /// <summary>
    /// collect shadows
    /// </summary>
    public RenderTexture collectShadows;

    /// <summary>
    /// reflection RT
    /// </summary>
    //[HideInInspector]
    public RenderTexture reflectionRT;

    /// <summary>
    /// ambient rt
    /// </summary>
    //[HideInInspector]
    public RenderTexture ambientRT;

    /// <summary>
    /// instance
    /// </summary>
    public static SqLightManager Instace { get; private set; }

    AmbientConstant lastAmbientConst;

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
        reflectionRT = SqUtility.CreateRT(Screen.width >> reflectionDownSample, Screen.height >> reflectionDownSample, 0, RenderTextureFormat.ARGB32, "Reflection RT", true, true);

        // create ambient rt
        ambientRT = SqUtility.CreateRT(Screen.width >> ambientDownSample, Screen.height >> ambientDownSample, 0, RenderTextureFormat.ARGB32, "Ambient RT", true);

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
        if (lastAmbientConst != ambientConst)
        {
            SetAmbientData(ambientConst);
        }

        lastAmbientConst = ambientConst;
    }
}
