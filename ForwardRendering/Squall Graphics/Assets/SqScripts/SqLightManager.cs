using System;
using System.Runtime.InteropServices;
using UnityEngine;

// for init sq light
public class SqLightManager : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern void InitSqLight(int _numDirLight, int _numPointLight, int _numSpotLight, IntPtr _collectShadows, IntPtr _reflectionRT);

    [DllImport("SquallGraphics")]
    static extern void SetPCFKernel(int _kernel);

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

    /// <summary>
    /// reflection down sample
    /// </summary>
    [Range(0, 3)]
    public int reflectionDownSample = 0;

    /// <summary>
    /// collect shadows
    /// </summary>
    public RenderTexture collectShadows;

    /// <summary>
    /// reflection RT
    /// </summary>
    public RenderTexture reflectionRT;

    /// <summary>
    /// instance
    /// </summary>
    public static SqLightManager Instace { get; private set; }

    // Start is called before the first frame update
    void Start()
    {
        Instace = this;
        InitLights();
        SetPCF();
    }

    void Update()
    {
        // need set pcf before late update
        SetPCF();
    }

    void OnDestroy()
    {
        SqUtility.SafeDestroyRT(ref collectShadows);
        SqUtility.SafeDestroyRT(ref reflectionRT);
    }

    void InitLights()
    {
        int downSample = 1;

        // create collect shadows 
        collectShadows = SqUtility.CreateRT(Screen.width >> downSample, Screen.height >> downSample, 0, RenderTextureFormat.ARGBHalf, "Collect Shadows");

        // create reflection rt
        int reflSize = Mathf.ClosestPowerOfTwo(Screen.width) >> reflectionDownSample;
        reflectionRT = SqUtility.CreateRT(reflSize, reflSize, 0, RenderTextureFormat.ARGB32, "Reflection RT", true, true);

        InitSqLight(maxDirectionalLight, maxPointLight, maxSpotLight, collectShadows.GetNativeTexturePtr(), reflectionRT.GetNativeTexturePtr());
    }

    void SetPCF()
    {
        SetPCFKernel(pcssKernel);
    }
}
