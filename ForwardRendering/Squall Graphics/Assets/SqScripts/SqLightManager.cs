using System;
using System.Runtime.InteropServices;
using UnityEngine;

// for init sq light
public class SqLightManager : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern void InitSqLight(int _numDirLight, int _numPointLight, int _numSpotLight, IntPtr _collectShadows, int _instance);

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
    /// collect shadows
    /// </summary>
    public RenderTexture collectShadows;

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
        if (collectShadows != null)
        {
            collectShadows.Release();
            DestroyImmediate(collectShadows);
        }
    }

    void InitLights()
    {
        int downSample = 1;

        // create collect shadows 
        collectShadows = new RenderTexture(Screen.width >> downSample, Screen.height >> downSample, 0, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear); 
        collectShadows.name = "Collect Shadows";
        collectShadows.Create();

        InitSqLight(maxDirectionalLight, maxPointLight, maxSpotLight, collectShadows.GetNativeTexturePtr(), collectShadows.GetInstanceID());
    }

    void SetPCF()
    {
        SetPCFKernel(pcssKernel);
    }
}
