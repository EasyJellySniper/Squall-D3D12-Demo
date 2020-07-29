using System;
using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// squall graphic manager
/// </summary>
public class SqGraphicManager : MonoBehaviour
{
    /// <summary>
    /// pcf kernel
    /// </summary>
    public enum PCFKernel
    {
        PCF3x3 = 0, PCF4x4, PCF5x5
    }

    [DllImport("SquallGraphics")]
    static extern bool InitializeSqGraphic(int _numOfThreads);

    [DllImport("SquallGraphics")]
    static extern void InitRayTracingInterface();

    [DllImport("SquallGraphics")]
    static extern void InitSqLight(int _numDirLight, int _numPointLight, int _numSpotLight, IntPtr _collectShadows, int _instance);

    [DllImport("SquallGraphics")]
    static extern void ReleaseSqGraphic();

    [DllImport("SquallGraphics")]
    static extern void UpdateSqGraphic();

    [DllImport("SquallGraphics")]
    static extern void RenderSqGraphic();

    [DllImport("SquallGraphics")]
    static extern int GetRenderThreadCount();

    [DllImport("SquallGraphics")]
    static extern void SetPCFKernel(int _kernel);

    /// <summary>
    /// instance
    /// </summary>
    public static SqGraphicManager instance;

    /// <summary>
    /// number of render threads
    /// </summary>
    [Range(2, 32)]
    public int numOfRenderThreads = 4;

    /// <summary>
    /// global aniso level
    /// </summary>
    [Range(1, 16)]
    public int globalAnisoLevel = 8;

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

    /// <summary>
    /// reseting frame
    /// </summary>
    [HideInInspector]
    public bool resetingFrame = false;

    PCFKernel lastPCF;

    void Awake()
    {
        if (instance != null)
        {
            Debug.LogError("Only one SqGraphicManager can be created.");
            enabled = false;
            return;
        }

        if (numOfRenderThreads < 2)
        {
            numOfRenderThreads = 2;
        }

        if (InitializeSqGraphic(numOfRenderThreads))
        {
            Debug.Log("[SqGraphicManager] Squall Graphics initialized.");
            instance = this;
        }
        else
        {
            Debug.LogError("[SqGraphicManager] Squall Graphics terminated.");
            enabled = false;
            return;
        }

        InitRayTracingInterface();
        InitLights();

        Debug.Log("[SqGraphicManager] Number of render threads: " + GetRenderThreadCount());
    }

    void OnApplicationQuit()
    {
        ReleaseSqGraphic();
    }

    void LateUpdate()
    {
        if (resetingFrame)
        {
            resetingFrame = false;
            return;
        }

        SetPCF();
        UpdateSqGraphic();
        RenderSqGraphic();
    }

    void InitLights()
    {
        var objs = gameObject.scene.GetRootGameObjects();
        bool hasShadowLight = false;

        for (int i = 0; i < objs.Length && !hasShadowLight; i++)
        {
            Light[] lights = objs[i].GetComponentsInChildren<Light>();

            foreach (var l in lights)
            {
                if (l.shadows != LightShadows.None)
                {
                    hasShadowLight = true;
                    break;
                }
            }
        }

        if (hasShadowLight)
        {
            SqLight.collectShadows = new RenderTexture(Screen.width, Screen.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
            SqLight.collectShadows.name = "Opaque Shadows";
            SqLight.collectShadows.Create();
        }

        InitSqLight(maxDirectionalLight, maxPointLight, maxSpotLight, SqLight.collectShadows.GetNativeTexturePtr(), SqLight.collectShadows.GetInstanceID());
        SetPCF();
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
