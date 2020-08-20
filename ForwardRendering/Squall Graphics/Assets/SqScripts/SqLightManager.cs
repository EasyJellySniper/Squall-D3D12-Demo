using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.SceneManagement;

// for init sq light
public class SqLightManager : MonoBehaviour
{
    /// <summary>
    /// pcf kernel
    /// </summary>
    public enum PCFKernel
    {
        PCF3x3 = 0, PCF4x4, PCF5x5
    }


    [DllImport("SquallGraphics")]
    static extern void InitSqLight(int _numDirLight, int _numPointLight, int _numSpotLight, IntPtr _collectShadows, int _instance);

    [DllImport("SquallGraphics")]
    static extern void SetPCFKernel(int _kernel);

    /// <summary>
    /// ray tracing shadow?
    /// </summary>
    public bool rayTracingShadow = false;

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
    /// pcf kernel
    /// </summary>
    public PCFKernel pcfKernal = PCFKernel.PCF4x4;
    PCFKernel lastPCF;

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
        int downSample = 0;
        if (rayTracingShadow)
        {
            downSample = 1;
        }

        // create collect shadows 
        collectShadows = new RenderTexture(Screen.width >> downSample, Screen.height >> downSample, 0, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear); 
        collectShadows.name = "Collect Shadows";
        collectShadows.Create();

        InitSqLight(maxDirectionalLight, maxPointLight, maxSpotLight, collectShadows.GetNativeTexturePtr(), collectShadows.GetInstanceID());
    }

    void SetPCF()
    {
        if (lastPCF != pcfKernal)
        {
            SetPCFKernel((int)pcfKernal);
        }

        lastPCF = pcfKernal;
    }
}
